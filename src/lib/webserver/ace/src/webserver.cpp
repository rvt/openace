#include "../webserver.hpp"

/* System. */
#include <stdio.h>

/* LWIP. */
#include "lwipopts.h"
#include "lwip/tcp.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
// #include "lwip/apps/mdns.h"

/* Pico. */
#include "pico/cyw43_arch.h"

/* GATAS. */
#include "ace/coreutils.hpp"

Webserver *webserver;

/** Needed for captive portal */
//** Captive portal currently not used until a real usecase arrives*/
extern struct tcp_pcb *tcp_input_pcb;
inline etl::map<uint32_t, uint32_t, 4> captiveCheck;

/* Other consts */
constexpr etl::string_view X_GATAS_METHOD_DELETE = "X-Method: DELETE"; // Custom HTTP header for method intent

static struct RequestContext_t
{
    void *connection;
    GATAS::ConfigPathString uri;
    enum
    {
        POST,
        DELETE
    } method;
    char buffer[256]; // If Json deserialisation fails, check if the JSON send is not bigger than this value
    size_t bufferPosition;
} requestContext;

/**
 * At this moment only one post request is supported at a time
 * Returns ERR_USE when another post is ongoing
 */
err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       u16_t http_request_len, int content_len, char *response_uri,
                       u16_t response_uri_len, u8_t *post_auto_wnd)
{
    LWIP_UNUSED_ARG(http_request_len);
    LWIP_UNUSED_ARG(content_len);
    LWIP_UNUSED_ARG(response_uri);
    LWIP_UNUSED_ARG(response_uri_len);
    etl::string_view sv_uri(uri);
    etl::string_view sv_http_request(http_request);
    if (sv_uri.starts_with("/api/"))
    {
        if (requestContext.connection == nullptr)
        {
            *post_auto_wnd = 1; // Must be set to 1
            requestContext =
                {
                    .connection = connection,
                    .uri = uri,
                    .method = RequestContext_t::POST,
                    .buffer = {},
                    .bufferPosition = 0};

            // LWiP doesn't handle DELETE requests, so the Frontend must add this as a header to indicate a DELETE request within a POST
            if (sv_http_request.find(X_GATAS_METHOD_DELETE) != etl::string_view::npos)
            {
                requestContext.method = RequestContext_t::DELETE;
            }
        }
        else
        {
            return ERR_USE; // Only one POST at a time
        }
    }
    return ERR_OK;
}

/**
 * Copy received data into the requestContext buffer
 * Note: httpd_post_receive_data can be called multiple times for one requets
 *
 * Returns ERR_MEM when not enough space in the buffer
 */
err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    if (requestContext.connection == connection)
    {
        if ((requestContext.bufferPosition + p->len + 1) > sizeof(requestContext.buffer))
        {
            return ERR_MEM; // Not enough space
        }
        auto copied = pbuf_copy_partial(p, requestContext.buffer + requestContext.bufferPosition, p->len, 0);
        if (copied != p->len)
        {
            requestContext.bufferPosition = 0;
            requestContext.buffer[0] = '\0';
            return ERR_BUF;
        }

        requestContext.bufferPosition += p->len;
    }
    pbuf_free(p);
    return ERR_OK;
}

/**
 * Handle the post finished request by calling the correct routines and doing the 'work'
 */
void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
    if (requestContext.connection != connection)
    {
        requestContext.connection = nullptr;
        return;
    }

    auto path = CoreUtils::parsePath(requestContext.uri);
    if (path.size() < 3)
    {
        return;
    }

    // Lookup can be cached externally if stable.
    auto *configModule = static_cast<Configuration *>(BaseModule::moduleByName(*webserver, path[1]));

    if (configModule)
    {
        if (requestContext.method == RequestContext_t::POST && requestContext.bufferPosition > 0)
        {
            // Null terminate the buffer, httpd_post_receive_data ensures there is room
            requestContext.buffer[requestContext.bufferPosition] = '\0';
            configModule->setData(requestContext.buffer, requestContext.uri);
        }
        else if (requestContext.method == RequestContext_t::DELETE)
        {
            configModule->deleteData(requestContext.uri);
        }
       
    }

    snprintf(response_uri, response_uri_len, "/ok.json");
    requestContext.connection = nullptr;
}

// ******************************************
// /API Get handling
// ******************************************
extern struct fsdata_file file_captive_html[];
extern struct fsdata_file file_ios_html[];
const uint8_t IS_ACCEPTED = 1 << 1;

int handle_captive(fs_file *file, const etl::ivector<GATAS::Modulename> &path)
{
    // How captive works. All devices test a URL and see what content is returned. If the unexpected content is returned, it's assumed it need to be captive:
    // For iOS: iOS loads hotspot-detect.html -> GaTas shows something different -> iOS Opens loading the page GaTas returns a landing page with redirect -> iOS still showing captive, follows redirect and shows GATAS
    // for Android: Android checks generate_204 -> GaTas returns something different -> Keep portal open, but service pages
    // For Android it would be nice to start using DHCP_OPT_CAPTIVE_PORTAL see dhcpserver.c
    uint32_t remoteAddress = tcp_input_pcb->remote_ip.addr;
    if (captiveCheck[remoteAddress] & IS_ACCEPTED)
    {
        return 0;
    }

    // if (!(captiveCheck[remoteAddress] & IS_CAPTIVE))
    // {
    //     const etl::vector captivePaths{ "generate_204", "gen_204", "hotspot-detect", "ios", "captive" };
    //     // Search for the pathFront in the captivePaths vector
    //     if (etl::find(captivePaths.cbegin(), captivePaths.cend(), path.front()) != captivePaths.cend())
    //     {
    //         captiveCheck[remoteAddress] |= IS_CAPTIVE;
    //     }
    // }

    const struct fsdata_file *captiveDocument;
    if (path.front() == "hotspot-detect") // IOS, set to accepted so the connection is accepted
    {
        captiveDocument = &file_ios_html[0];
        captiveCheck[remoteAddress] |= IS_ACCEPTED;
    }
    else if (path.front() == "generate_204" || path.front() == "gen_204") // Android, accept the captive.
    {
        captiveDocument = &file_captive_html[0];
        captiveCheck[remoteAddress] |= IS_ACCEPTED;
    }
    else
    {
        return 0;
    }

    // Setup file for sending
    file->data = (const char *)captiveDocument[0].data;
    file->len = captiveDocument[0].len;
    file->index = captiveDocument[0].len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return 1;
}

int fs_open_custom(fs_file *file, const char *name)
{
    constexpr auto MAX_CONTENT_SIZE = 1536;

    auto pathString = GATAS::ConfigPathString(name);
    auto path = CoreUtils::parsePath(pathString);

    // Test if this is an request for api data requests
    if (path.size() == 0)
    {
        return 0;
    }

    // TODO: Test and handle captive portal
    // Was stable for iOS, but for Android it worked fine okish, but it didn't feel intuitive yet
    // if (handle_captive(file, path))
    // {
    //     return 1;
    // }

    // GaTas API calls
    if (path.back() != "json" || path.front() != "api")
    {
        return 0;
    }

    BaseModule *module = BaseModule::moduleByName(*webserver, path[1].c_str());
    if (module == nullptr)
    {
        return 0;
    }

    memset(file, 0, sizeof(fs_file));
    file->pextension = mem_malloc(MAX_CONTENT_SIZE + 1);
    if (file->pextension)
    {
        // Get the response and get it's size in the large buffer first
        etl::string_ext response((char *)file->pextension, MAX_CONTENT_SIZE);
        etl::string_stream stream(response);
        module->getData(stream, pathString);

        file->data = (const char *)file->pextension;
        file->len = response.size();
        file->index = file->len; // We set index to len to indicate that the complete file has been read and the server can send and close the response
        // file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
        return 1;
    }
    else
    {
        webserver->statistics.memAllocErr += 1;
    }
    return 0;
}

void fs_close_custom(fs_file *file)
{
    if (file && file->pextension)
    {
        mem_free(file->pextension);
        file->pextension = nullptr;
    }
}

int fs_read_custom(struct fs_file *file, char *buffer, int count)
{
    GATAS_INFO("not implemented in this example configuration");
    LWIP_ASSERT("not implemented in this example configuration", 0);
    LWIP_UNUSED_ARG(file);
    LWIP_UNUSED_ARG(buffer);
    LWIP_UNUSED_ARG(count);
    /* Return
       - FS_READ_EOF if all bytes have been read
       - FS_READ_DELAYED if reading is delayed (call 'tcpip_callback(callback_fn, callback_arg)' when done) */
    /* all bytes read already */
    return FS_READ_EOF;
}

void Webserver::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    const auto registeredModules = BaseModule::registeredModules();
    stream << "{";
    stream << "\"modules\":{";

    for (auto it = registeredModules.cbegin(); it != registeredModules.cend(); it++)
    {
        stream << "\"" << it->first << "\": {\"poststatus\": " << (uint8_t)(it->second.result) << "}";
        if (etl::next(it) != registeredModules.cend())
        {
            stream << ",";
        }
    }
    stream << "}";
    stream << ",\"memAlloc:err\":" << statistics.memAllocErr;
    stream << "}";
}

GATAS::PostConstruct Webserver::postConstruct()
{
    return GATAS::PostConstruct::OK;
}

void Webserver::start()
{
    webserver = this;
    getBus().subscribe(*this);
};

void Webserver::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void Webserver::on_receive(const GATAS::WifiConnectionStateMsg &wcs)
{
    // Only start the webserver after WIFI has been connected.
    // We have seen halts from the microcontroller when the website did a request to the webserver while WIFI was not fully up yet.
    // This runs in a general stack (properly of the message bus)
    if (wcs.wifiMode != GATAS::WifiMode::NC && !httpdInitialised)
    {
        httpd_init();
        httpdInitialised = true;
    }
    else if (wcs.wifiMode == GATAS::WifiMode::NC)
    {
        // httpd server in LWiP was never designed for de-init, I don't think this is possible.
        // This might have consequences if GA/TAS
        // Idea: May be after the first HTTP call we can capture the PCB??
        // We don;t want to keep calling httpd_init because this would possible leak memory
    }
}
