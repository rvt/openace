import { El } from "@frameable/el";
import "./monitormodule";
import store from "./store";
import { icon } from "./utils";

class OpenAceModules extends El {
  created() {
    this.state = this.$observable({
      modules: [],
      whatToShow: "modules",
      selectedModule: 0,
      enabled: [],
      restartDlg: false,
    });
    // All modules that can be monitored (Some time we should automated this by reading this from the Micocontroller)
    this.monitorable = [
      "ADSL",
      "Flarm",
      "GDLoverUDP",
      "Gdl90Service",
      "GpsDecoder",
      "Ogn1",
      "PicoRtc",
      "RadioTunerTx",
      "RadioTunerRx",
      "Sx1262_0",
      "Sx1262_1",
      "UbloxM8N",
      "ADSBDecoder",
      "Dump1090Client",
      "Bmp280",
      "Config",
      "AircraftTracker",
      "WifiService",
      "DataPort",
      "AirConnect",
      "Bluetooth"
    ];
    this.configurable = ["WifiService", "ADSBDecoder", "GDLoverUDP", "Dump1090Client", "Bmp280", "Sx1262_0", "Sx1262_1", "AircraftTracker"];
    this.enablers = [
      "ADSBDecoder",
      "Ogn1",
      "Flarm",
      "GDLoverUDP",
      "Gdl90Service",
      "Dump1090Client",
      "Bmp280",
      "ADSL",
      "SerialADSB",
      "Sx1262_1",
      "Sx1262_0",
      "UbloxM8N",
      "RadioTunerRx",
      "RadioTunerTx",
      "DataPort",
      "AirConnect",
      "Bluetooth"
    ];
    this.info = {
      WifiService: (html) =>
      html`Provides both Access Point and Client modes, allowing OpenAce to connect to or create a network. It can connect to multiple networks based on availability.`,
      AircraftTracker: (html) =>
      html`Tracks all received aircraft and updates their positions when no new data is received. Sends a 1-second heartbeat with updated positions for each aircraft for a configurable duration.`,
      Webserver: (html) => html`Hosts this page and facilitates configuration changes.`,
      DataPort: (html) => html`Generates NMEA sentences compatible with DataPort, enabling EFBs like SkyDemon to receive traffic and ownship information via Bluetooth or AirConnect.`,
      AirConnect: (html) => html`Sends all DataPort messages over TCP. Prefer Bluetooth if supported by your EFB. Listens on port 2000 and sends data upon connection. Note: ForeFlight may require proprietary port negotiation.`,
      Bluetooth: (html) => html`Transmits all DataPort messages over Bluetooth, providing NMEA datastreams to external devices. EFBs like SkyDemon can connect to OpenAce via Bluetooth.`,
      Gdl90Service: (html) => html`Generates GDL90 messages. Requires a module like GDLoverUDP to receive them on external devices.`,
      GDLoverUDP: (html) => html`Transmits GDL90 messages over UDP to external devices.`,
      Flarm: (html) => html`Sends and receives Flarm protocol messages.`,
      Ogn1: (html) => html`Sends and receives OGN protocol messages.`,
      ADSL: (html) => html`Sends and receives ADS-L protocol messages.`,
      ADSBDecoder: (html) =>
      html`Receives ADS-B (extended squitter) messages. Requires an input module like SerialADSB or Dump1090Client.`,
      SerialADSB: (html) => html`Receives ADS-B messages from hardware like the GNS5892. Requires an ADSB Decoder to process messages.`,
      Dump1090Client: (html) => html`Receives ADS-B messages from Dump1090. Requires an ADSB Decoder to process messages.`,
      Bmp280: (html) => html`Reads atmospheric pressure using the Bmp280 hardware.`,
      AceSpi: (html) => html`Core module for controlling SPI access between different modules.`,
      Config: (html) => html`Core module for receiving and storing configurations.`,
      GpsDecoder: (html) => html`Core module for decoding GPS NMEA messages.`,
      UbloxM8N: (html) => html`Configures uBlox8 or similar hardware devices.`,
      PicoRtc: (html) => html`Reads GPS messages and handles accurate time tracking for various protocols.`,
      Sx1262_0: (html) => html`Radio module 1. Sends and receives ADS-L, OGN, and Flarm protocols.`,
      Sx1262_1: (html) => html`Radio module 2. Sends and receives ADS-L, OGN, and Flarm protocols.`,
      RadioTunerRx: (html) => html`Manages timings for receiving multiple protocols over one or more radios (Flarm, OGN, and ADS-L).`,
      RadioTunerTx: (html) => html`Manages sending regular position messages over different protocols like Flarm, OGN, and ADS-L.`,
    };
  }

  mounted() {
    this._running = false;
    this._fetchData();
  }

  unmounted() {
    this._running = false;
    clearTimeout(this.timer);
  }

  _restart() {
    store.restart();
    this.state.restartDlg = false;
  }

  _usbBoot() {
    store.usbBoot();
    this.state.usbBootDlg = false;
  }

  _restartButton(html) {
    return html`<button class="btn xs" onclick=${() => (this.state.restartDlg = true)}>Restart</button>`;
  }
  _usbBootButton(html) {
    return html`<button class="btn xs" onclick=${() => (this.state.usbBootDlg = true)}>Upload Firmware</button>`;
  }

  _restartAreYouSureDlg(html) {
    return html` <div class="modal show">
      <div class="modal-content mw-400 rounded">
        <article class="accent-light shadow">
          <header>
            <h4>Restart OpenAce?</h4>
          </header>
          <div class="overflow-auto accent-primary" style="color: black">
            <!-- Quick hack to make text black on Safari Desktop -->
            <p>
              The connection will be temporary disconnected. Any unsaved data will be available after restart.<br />
              Are you sure?
            </p>
          </div>
          <footer class="px-2 jc-end">
            <button type="button" class="btn btn-error sm ml-1 md-ml-3" onclick=${this._restart}>Yes</button>
            <button type="button" class="btn btn-primary sm ml-1 md-ml-3" onclick=${() => (this.state.restartDlg = false)}>No</button>
          </footer>
        </article>
      </div>
    </div>`;
  }

  _usbBootDlgAreYouSureDlg(html) {
    return html` <div class="modal blur show">
      <div class="modal-content mw-400 rounded">
        <article class="shadow accent-light">
          <header>
            <h4>Start Firmware mode?</h4>
          </header>
          <div class="overflow-auto accent-primary" style="color: black">
            <!-- Quick hack to make text black on Safari Desktop -->
            <p>
              To update OpenAce, make sure it is connected to your computer with a USB cable through the <strong>Microcontroller port</strong> (the charge port
              won’t work for this step).
            </p>
            <p>
              When OpenAce restarts, it will show up as a new drive on your computer. Once you see the drive, simply drag and drop the
              <strong>OpenAce.uf2</strong> file onto it. After a moment, the device will restart automatically, and OpenAce will be ready to use again.
            </p>
          </div>
          <footer class="px-2 jc-end">
            <button type="button" class="btn btn-error sm ml-1 md-ml-3" onclick=${this._usbBoot}>Yes</button>
            <button type="button" class="btn btn-primary sm ml-1 md-ml-3" onclick=${() => (this.state.usbBootDlg = false)}>No</button>
          </footer>
        </article>
      </div>
    </div>`;
  }

  _postConstructToString(value) {
    const errorMap = {
      0: "Never Loaded",
      1: "Ok",
      2: "OpenAce::PostConstruct failed",
      3: "Memory error",
      4: "A dependency was not found",
      5: "xQueue error",
      6: "Task Error",
      7: "Hardware not found",
      8: "Hardware error",
      9: "Network error",
      10: "Configuration error",
      11: "Timer error",
      12: "Mutex error",
    };
    return errorMap[value] || "Unknown error";
  }

  _fetchData() {
    store
      .fetch("/api/Webserver.json")
      .then((data) => {
        this.state.modules.length = 0;
        for (var prop in data.modules) {
          this.state.modules.push({ name: prop, ...data.modules[prop] });
        }
      })
      .catch((e) => {
        this.state.modules.length = 0;
      })
      .then((data) => {
        return store.getModuleData("modules").then((data) => {
          this.state.enabled = (data ?? "").split(",");
          return data;
        });
      })
      .finally(() => {
        this.$update();
        if (this._running) {
          this.timer = setTimeout(() => {
            this._fetchData();
          }, 5000);
        }
      });
  }

  _monitorModule(module) {
    this.state.selectedModule = module;
    this.state.whatToShow = "monitor";
  }

  _configureModule(module) {
    this.state.selectedModule = module;
    this.state.whatToShow = "configure";
  }

  _toggleModule(moduleName) {
    const index = this.state.enabled.indexOf(moduleName);
    if (index !== -1) {
      this.state.enabled.splice(index, 1);
    } else {
      this.state.enabled.push(moduleName);
    }

    store.updateModuleData("modules", this.state.enabled.join(",")).then((data) => {
      return data;
    });
  }

  _showConfigureModule(html) {
    let module = this.state.selectedModule;
    return html`
      <${module}-config key="${module}-config" close=${() => (this.state.whatToShow = "modules")}></${module}-config>
    `;
  }

  _showModuleStatus(html) {
    return html`
      <monitor-module key="config" selected=${this.state.selectedModule}></monitor-module>
      <button class="btn" onclick=${() => (this.state.whatToShow = "modules")}>Close</button>
    `;
  }

  _row(html, item) {
    let monitorBtn =
      item.poststatus == 1 && this.monitorable.includes(item.name)
        ? html`<button class="btn xs" onclick=${() => this._monitorModule(item.name)}>👀</button>`
        : "";
    let configureBtn = this.configurable.includes(item.name)
      ? html`<button class="btn xs" ${this.configurable.includes(item.name) ? "" : "disabled"} onclick=${() => this._configureModule(item.name)}>🛠️</button>`
      : "";

    let toggleBtn = this.state.enabled.includes(item.name)
      ? html`<button class="btn xs btn-success" onclick=${() => this._toggleModule(item.name)}>Ⓘ</button>`
      : html`<button class="btn xs btn-error" onclick=${() => this._toggleModule(item.name)}>ⓧ</button>`;

    let enabledBtn = this.enablers.includes(item.name) ? toggleBtn : "";

    let info = "";
    if (this.info[item.name]) {
      info = html` <label class="btn sm btn-medium btn-link p-0 circle mt-n1">
        ${html.raw(icon.help)}
        <p class="tooltip rounded shadow o-90 p-2 bg-dark color-light mw-300 sm outset-bottom inset-right text-left mh-200 overflow-auto">
          ${this.info[item.name](html)}
        </p>
      </label>`;
    }

    return html` <tr>
      <th style="width:25%" scope="row">${item.name} ${info}</th>
      <td>${this._postConstructToString(item.poststatus)}</td>
      <td>${enabledBtn} ${configureBtn} ${monitorBtn}</td>
    </tr>`;
  }

  _filteredItems() {
    return this.state.modules.filter((i) => !i.name.startsWith("_"));
  }

  _showModuleOverview(html) {
    let items = this._filteredItems();
    return html`
      <div class="grid md-columns-2 lg-columns-3 ">
        <div>${this._restartButton(html)}</div>
        <div>${this._usbBootButton(html)}</div>
      </div>
      ${this.state.restartDlg ? this._restartAreYouSureDlg(html) : ""} ${this.state.usbBootDlg ? this._usbBootDlgAreYouSureDlg(html) : ""}
      <div class="section">
        <table>
          <tbody>
            ${items.map((item) => html` ${this._row(html, item)} `)}
          </tbody>
        </table>
      </div>
    `;
  }

  render(html) {
    let pageContent;
    switch (this.state.whatToShow) {
      case "modules":
        pageContent = this._showModuleOverview(html);
        break;
      case "monitor":
        pageContent = this._showModuleStatus(html);
        break;
      case "configure":
        pageContent = this._showConfigureModule(html);
        break;
      default:
        pageContent = "Not available";
    }

    return pageContent;
  }
}
customElements.define("openace-modules", OpenAceModules);
