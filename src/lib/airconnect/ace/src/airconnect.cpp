#include <stdio.h>

#include "../airconnect.hpp"

GATAS::PostConstruct AirConnect::postConstruct()
{
    return GATAS::PostConstruct::OK;
}

void AirConnect::start()
{
    getBus().subscribe(*this);
};

void AirConnect::stop()
{
    getBus().unsubscribe(*this);
    tcp_server_close();
};

void AirConnect::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"connectedClients\":" << connectedClients.size();
    stream << ",\"toManyClients\":" << statistics.toManyClients;
    stream << ",\"tcpWriteErr\":" << statistics.tcpWriteErr;
    stream << ",\"newClientConnection\":" << statistics.newClientConnection;
    stream << "}\n";
}

/**
 * Receive dataport messages and send it to all clients
 * TODO: Change it such that each connected client can receive the GATAS::DataPortMsg &msg
 */
void AirConnect::on_receive(const GATAS::DataPortMsg &msg)
{
    cyw43_arch_lwip_begin();
    for (auto &it : connectedClients)
    {
        if (it.buffer.empty())
        {
            it.buffer.push(msg.sentence.c_str(), msg.sentence.size());
            err_t err = tcp_write(it.pcb, msg.sentence.c_str(), msg.sentence.size(), TCP_WRITE_FLAG_COPY);
            if (err != ERR_OK)
            {
                statistics.tcpWriteErr += 1;
            }
        }
        else if (msg.sentence.size() <= it.buffer.available())
        {
            it.buffer.push(msg.sentence.c_str(), msg.sentence.size());
        }
        else
        {
            it.bufferOverrunErr += 1;
        }
    }
    cyw43_arch_lwip_end();
}

void AirConnect::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

/**
 * Handle the sent callback, will advance the buffer by calling accepted and
 * send more data if in the buffer
 *
 *  No need for a mutex here because lwip_begin() / lwip_end() ensures sent is not called
 */
err_t AirConnect::tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    (void)pcb;

    TcpClientState *con_state = (TcpClientState *)arg;
    err_t err = ERR_OK;
    con_state->buffer.accepted(len);

    auto [part, peekLength] = con_state->buffer.peek();
    if (peekLength > 0)
    {
        err = tcp_write(pcb, part, peekLength, TCP_WRITE_FLAG_COPY);
        // Direct flush at larger packages to ensure EFB is updated on time
        if (peekLength > 400)
        {
            tcp_output(pcb);
        }
    }

    return err;
}

/**
 * Handle poll callback, if any data in the queue, send it
 */
err_t AirConnect::tcp_server_poll(void *arg, struct tcp_pcb *pcb)
{
    (void)pcb;

    TcpClientState *con_state = (TcpClientState *)arg;
    err_t err = ERR_OK;

    auto [part, peekLength] = con_state->buffer.peek();
    if (peekLength)
    {
        err = tcp_write(pcb, part, peekLength, TCP_WRITE_FLAG_COPY);
        // Direct flush at larger packages to ensure EFB is updated on time
        if (peekLength > 400)
        {
            tcp_output(pcb);
        }
    }
    return err;
}

/**
 * Remove all empty PCB's
 */
void AirConnect::removeEmptyPCB()
{
    for (auto it = connectedClients.begin(); it != connectedClients.end();)
    {
        if (it->pcb == nullptr)
        {
            it = connectedClients.erase(it); // Remove matching client
        }
        else
        {
            ++it; // Continue to next item
        }
    }
}

/**
 * Stop and close a client pcb
 */
err_t AirConnect::tcp_close_client_connection(TcpClientState &con_state, err_t close_err)
{
    if (con_state.pcb)
    {
        tcp_arg(con_state.pcb, NULL);
        tcp_poll(con_state.pcb, NULL, 0);
        tcp_sent(con_state.pcb, NULL);
        tcp_recv(con_state.pcb, NULL);
        tcp_err(con_state.pcb, NULL);
        err_t err = tcp_close(con_state.pcb);
        if (err != ERR_OK)
        {
            tcp_abort(con_state.pcb);
            close_err = ERR_ABRT;
        }
        con_state.pcb = nullptr;
    }
    return close_err;
}

/**
 * Handle any requests for 'passwords' if the client requests for it
 */
err_t AirConnect::tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    (void)err;
    TcpClientState *con_state = (TcpClientState *)arg;
    if (!p)
    {
        tcp_close_client_connection(*con_state, ERR_OK);
        con_state->airConnect->removeEmptyPCB();
        return ERR_OK;
    }

    //  printf("tcp_server_recv %d err %d\n", p->tot_len, err);
    if (p->tot_len > 0)
    {
        char buffer[12] = {0};
        pbuf_copy_partial(p, buffer, p->tot_len > sizeof(buffer) - 1 ? sizeof(buffer) - 1 : p->tot_len, 0);

        //  printf("tcp_server_recv: %s\n", buffer);
        if (strncmp((char *)buffer, "1234", 4) == 0)
        {
            //  printf("Password received!\n");
            err_t err = tcp_write(pcb, "AOK", 3, TCP_WRITE_FLAG_COPY); // According to some, no \r\n  after AOK
            tcp_output(pcb);
            if (err != ERR_OK)
            {
                // printf("failed to write AOK %d\n", err);
                tcp_close_client_connection(*con_state, ERR_VAL);
                con_state->airConnect->removeEmptyPCB();
                return ERR_VAL;
            }
        }
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}

/**
 * any error to the connection will close the client
 */
void AirConnect::tcp_server_err(void *arg, err_t err)
{
    (void)arg;
    if (err != ERR_ABRT)
    {
        TcpClientState *con_state = (TcpClientState *)arg;
        tcp_close_client_connection(*con_state, err);
        con_state->airConnect->removeEmptyPCB();
    }
}

/**
 * Accept an incoming client connections by adding it to the list of total connectionss
 */
err_t AirConnect::tcp_server_accept(void *arg, tcp_pcb *client_pcb, err_t aerr)
{
    if (aerr != ERR_OK || client_pcb == nullptr)
    {
        return ERR_VAL;
    }

    AirConnect *airConnect = static_cast<AirConnect *>(arg);
    err_t err;

    if (airConnect->connectedClients.full())
    {
        airConnect->statistics.toManyClients += 1;
        err = ERR_MEM;
    }
    else
    {
        airConnect->statistics.newClientConnection += 1;
        // printf("New Client %d\n", airConnect->connectedClients.size());

        TcpClientState *newConnection = &airConnect->connectedClients.emplace_back(client_pcb, airConnect);
        tcp_arg(client_pcb, newConnection);

        tcp_recv(client_pcb, tcp_server_recv);
        tcp_err(client_pcb, tcp_server_err);
        tcp_sent(client_pcb, tcp_server_sent);    // Called when data was successfully received by remote host
        tcp_poll(client_pcb, tcp_server_poll, 1); // Call back poll once a second (1 would be twice a second) for more data
        tcp_set_flags(client_pcb, TF_ACK_NOW);    // <-- seems to be a must?
        err = ERR_OK;
    }

    return err;
}

/**
 * Start the server PCB to accept TCP connections on the AIRCONNECT_PORT port
 */
bool AirConnect::tcp_server_start()
{
    cyw43_arch_lwip_begin();

    struct tcp_pcb *pcb = nullptr;
    pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb)
    {
        cyw43_arch_lwip_end();
        return false;
    }

    err_t err = tcp_bind(pcb, IP_ANY_TYPE, AIRCONNECT_PORT);
    if (err == ERR_USE)
    {
        //        printf("Port %u is already in use\n", AIRCONNECT_PORT);
        tcp_close(pcb);
        cyw43_arch_lwip_end();
        return false;
    }
    else if (err != ERR_OK)
    {
        //        printf("tcp_bind failed with error: %d\n", err);
        tcp_close(pcb);
        cyw43_arch_lwip_end();
        return false;
    }

    serverPcb = tcp_listen_with_backlog(pcb, 4);
    if (!serverPcb)
    {
        //        printf("Failed to listen\n");
        tcp_close(pcb);
        cyw43_arch_lwip_end();
        return false;
    }

    // tcp_nagle_disable(serverPcb);
    tcp_arg(serverPcb, this);
    tcp_accept(serverPcb, tcp_server_accept);

    cyw43_arch_lwip_end();
    return true;
}

/**
 * Close both the server and all clients
 * Call this after the unsubscription to the message bus has ben executed so no more data is received
 */
void AirConnect::tcp_server_close()
{
    cyw43_arch_lwip_begin();

    // Stop the server and this accepting any connections
    if (serverPcb)
    {
        tcp_arg(serverPcb, nullptr);
        tcp_close(serverPcb);
        serverPcb = nullptr;
    }

    // Close and stop all clients
    for (auto &it : connectedClients)
    {
        tcp_close_client_connection(it, ERR_OK);
    }

    cyw43_arch_lwip_end();
}

/**
 * Track wifi connects and disconnect
 */
void AirConnect::on_receive(const GATAS::WifiConnectionStateMsg &wcs)
{
    wifiConnected = wcs.connected;
}

/**
 * Starts and stop LWiP when wifi connects or disconnects to cleanup any resources
 */
void AirConnect::on_receive(const GATAS::IdleMsg &msg)
{
    (void)msg;
    if (wifiConnected && !serverPcb)
    {
        tcp_server_start();
    }

    if (!wifiConnected && serverPcb != nullptr)
    {
        tcp_server_close();
    }
}
