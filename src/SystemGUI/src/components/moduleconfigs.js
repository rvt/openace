import { El } from "@frameable/el";
import store from "./store";
import JustValidate from "just-validate";
import { icon, portValidation, ipValidation, ssidValidation, passwordValidation } from "./utils";

class ModuleConfig extends El {
  _initForm(dataFuture) {
    this.state = this.$observable({ data: {} });
    this.copyOfData = {};
    return dataFuture
      .then((data) => {
        Object.assign(this.state.data, data);
        Object.assign(this.copyOfData, data);
        this._setFormData(this.state.data);
        return data;
      })
      .catch((e) => {
        Object.assign(this.state.data, {});
      });
  }

  _resetForm() {
    Object.assign(this.state.data, this.copyOfData);
    this._setFormData(this.state.data);
  }

  buttonArray(html) {
    return html` <!-- Buttons -->
      <div class="section grid md-columns-3 lg-columns-3">
        <input class="btn" value="Cancel" onclick=${this.close} />
        <input class="btn" value="Reset" onclick=${this._resetForm} />
        <input class="btn btn-primary" type="submit" value="Save" />
      </div>`;
  }
}

class AircraftTrackerConfig extends ModuleConfig {
  created() {
    this._initForm(store.getModuleData("AircraftTracker"));
  }

  mounted() {
    const validator = new JustValidate(this.$refs.form);

    validator.onSuccess((event) => {
      const data = this._getFormData();
      store.updateModuleData("AircraftTracker", { ...this.copyOfData, ...data }).then(() => {
        this.close();
      });
    });
  }

  _setFormData(data) {}

  _getFormData() {
    return {};
  }

  render(html) {
    return html`
      <h4>Configuration of the Aircraft Tracker</h4>
      <p>
        Aircraft tracker receives all aircraft from ADSB, ADSL and other enabled protocols and keeps track of them. It will send on a 1 second heartbeat new
        calculated positions (when enabled) and will ensure that attached EFB's get regular updates.
      </p>
      <form ref="form" autocomplete="off" novalidate="novalidate">
        <div class="section grid md-columns-2 lg-columns-2">
          <strong>Update possibility follows soon</strong>
        </div>
      </form>
    `;
  }
}
customElements.define("aircrafttracker-config", AircraftTrackerConfig);

class WifiServiceConfig extends ModuleConfig {
  created() {
    this._initForm(store.getModuleData("WifiService"));
    this.clientsIds = [0, 1, 2, 3];
  }

  mounted() {
    const validator = new JustValidate(this.$refs.form);

    validator
      .addField(this.$refs.apssid, [
        {
          rule: "required",
        },
        ...ssidValidation,
      ])
      .addField(this.$refs.appassword, [
        {
          rule: "required",
        },
        ...passwordValidation,
      ])
      .onSuccess((event) => {
        const data = this._getFormData();
        store.updateModuleData("WifiService", { ...this.copyOfData, ...data }).then(() => {
          this.close();
        });
      });

    this.clientsIds.forEach((idx) => {
      validator.addField(this.$refs[`apssid_${idx}`], [...ssidValidation]).addField(this.$refs[`appassword_${idx}`], [...passwordValidation]);
    });
  }

  _setFormData(data) {
    // Get Access Point
    if (data == null || data.ap == null || data.ap.password == null || data.ap.password.length < 8 || data.ap.ssid == null || data.ap.ssid.length < 4) {
      this.$refs.apssid.value = "GATAS";
      this.$refs.appassword.value = "12345678";
    } else {
      this.$refs.apssid.value = data.ap.ssid;
      this.$refs.appassword.value = data.ap.password;
    }

    this.$refs.apDisabled.checked = data.ap.disabled;

    // Get Clients
    if (data != null && data.clients != null) {
      data.clients.forEach((client, idx) => {
        this.$refs[`apssid_${idx}`].value = client.ssid;
        this.$refs[`appassword_${idx}`].value = client.password;
      });
    }
  }

  _getFormData() {
    let clients = [];
    this.clientsIds.forEach((idx) => {
      if (this.$refs[`apssid_${idx}`].value && this.$refs[`appassword_${idx}`].value) {
        clients.push({ ssid: this.$refs[`apssid_${idx}`].value, password: this.$refs[`appassword_${idx}`].value });
      }
    });

    return {
      ap: {
        ssid: this.$refs.apssid.value,
        password: this.$refs.appassword.value,
        disabled: this.$refs.apDisabled.checked,
      },
      clients: clients,
    };
  }

  render(html) {
    return html`
      <h4>Configuration of the Wifi Service</h4>
      <p>
        This module connects to a Wifi Network in the following order:<br>
        <ul>
          <li> Try one of the listed networks
          <li> If non of the networks could be connected to, setup an Access Point
          </ul>
      </p>
      <form ref="form" autocomplete="off" novalidate="novalidate">
         <hr>
         <h5>Client configuration</h5>
         <p>GaTas will try to connect to each of the configured access points, if no content is possible the an Access Point will be created</p>
        <div class="grid md-columns-2 lg-columns-2">
            ${this.clientsIds.map(
              (id) => html`
                <div class="row g-0">
                  <div class="col-10">
                    <label for="ssid">
                      SSID ${id}:
                      <input type="text" id="apssid_${id}" ref="apssid_${id}" placeholder="SSID" } />
                    </label>
                    <label for="ssid">
                      Password ${id}:
                      <input type="text" id="appassword_${id}" ref="appassword_${id}" placeholder="Password" } />
                    </label>
                  </div>
                </div>
              `,
            )}
          </div>
          <hr>
        <h5>Access Point configuration</h5>
        <p>Access point configuration. This AP will be created if GaTas could not connect to any other Access Points.
          <div class="alert alert-warning">
            This access point does not support routing to the internet.
          </div>
        </p>
        <div class="row g-0">
            <div class="col-10">
            <label for="apDisabled">
              <input type="checkbox" id="" id="apDisabled" ref="apDisabled" placeholder="1" />
              When disabled, only attempts to an Access point will be done
            </label>
          <label for="ssid">
            SSID:
            <input type="text" id="apssid" ref="apssid" placeholder="SSID" />
          </label>
          <label for="ssid">
            Password:
            <input type="text" id="appassword" ref="appassword" placeholder="Password" />
          </label>
        </div>
        </div>

        ${this.buttonArray(html)}
      </form>
    `;
  }
}
customElements.define("wifiservice-config", WifiServiceConfig);

class ADSBDecoderConfig extends ModuleConfig {
  created() {
    this.filterAltitudes = [0, 500, 1000, 1500, 2000, 2500, 3000, 5000, 10000, 15000, 50000];
    this._initForm(store.getModuleData("ADSBDecoder"));
  }

  mounted() {
    const validator = new JustValidate(this.$refs.form);

    validator
      .addField(this.$refs.filterAbove, [
        {
          rule: "required",
        },
      ])
      .addField(this.$refs.filterBelow, [
        {
          rule: "required",
        },
      ])
      .onSuccess((event) => {
        const data = this._getFormData();
        store.updateModuleData("ADSBDecoder", { ...this.copyOfData, ...data }).then(() => {
          this.close();
        });
      });
  }

  _setFormData(data) {
    this.$refs[`filterAbove${data.filterAbove}`].selected = true;
    this.$refs[`filterBelow${data.filterBelow}`].selected = true;
  }

  _getFormData() {
    return {
      filterAbove: this.$refs.filterAbove.value,
      filterBelow: this.$refs.filterBelow.value,
    };
  }

  render(html) {
    return html`
      <h4>Configuration of the ADSB Decoder</h4>
      <p>
        This module processes the incoming ADS-B received over TCP, Serial or other methods. This module must be enabled next to a module that received data.
      </p>
      <form ref="form" autocomplete="off" novalidate="novalidate">
        <div class="section grid md-columns-2 lg-columns-2">
          <label for="filterAbove">
            filterAbove:
            <select ref="filterAbove" required>
              ${this.filterAltitudes.map((item) => html` <option ref="filterAbove${item}">${item}</option>`)}
            </select>
            <small><br />Removes all traffic more then ${this.state.data.filterAbove}ft above your own altitude</small>
          </label>
          <label for="filterBelow">
            filterBelow:
            <select ref="filterBelow" required>
              ${this.filterAltitudes.map((item) => html` <option ref="filterBelow${item}">${item}</option>`)}
            </select>
            <small><br />Removes all traffic more then ${this.state.data.filterBelow}ft below your own altitude</small>
          </label>
        </div>
        <br />
        <div class="alert alert-warning">
          ${html.raw(icon.warning)} Increasing the filter below and filter above settings will increase the load on the system, thus degrading the systems
          performance. Don't set the value to high/low.
        </div>

        ${this.buttonArray(html)}
      </form>
    `;
  }
}
customElements.define("adsbdecoder-config", ADSBDecoderConfig);

class Bmp280Config extends ModuleConfig {
  created() {
    this._initForm(store.getModuleData("Bmp280"));
  }

  mounted() {
    const validator = new JustValidate(this.$refs.form);

    validator
      .addField(this.$refs.compensation, [
        {
          rule: "required",
        },
        {
          rule: "integer",
        },
        {
          rule: "minNumber",
          value: -500,
        },
        {
          rule: "maxNumber",
          value: 500,
        },
      ])
      .onSuccess((event) => {
        const data = this._getFormData();
        store.updateModuleData("Bmp280", { ...this.copyOfData, ...data }).then(() => {
          this.close();
        });
      });
  }

  _setFormData(data) {
    this.$refs.compensation.value = data.compensation;
  }

  _getFormData() {
    return {
      compensation: this.$refs.compensation.value,
    };
  }

  render(html) {
    return html`
      <h4>Configuration of the Bmp280 pressure sensor</h4>
      <p>This module reads the Bmp280 pressure sensor and makes the data available to other modules that requires them.</p>

      <form ref="form" autocomplete="off" novalidate="novalidate">
        <div class="section grid md-columns-2 lg-columns-2">
          <label for="compensation">
            Compensation:
            <input type="text" id="compensation" ref="compensation" placeholder="0" } />
          </label>
        </div>
        <div class="alert alert-primary">Compensation allows to add an offset to the measured pressure for more accurate readings.</div>

        ${this.buttonArray(html)}
      </form>
    `;
  }
}

customElements.define("bmp280-config", Bmp280Config);

class GDLoverUDPConfig extends ModuleConfig {
  created() {
    this.ports = [0, 1, 2, 3];
    this.ips = [0, 1, 2, 3];
    this._initForm(store.getModuleData("GDLoverUDP")).then((data) => {
      this.$update();
    });
  }

  mounted() {
    const validator = new JustValidate(this.$refs.form);

    this.ports.forEach((port) => {
      validator.addField(this.$refs["port_" + port], portValidation);
    });
    this.ips.forEach((port) => {
      validator.addField(this.$refs["ip_" + port], ipValidation);
      validator.addField(this.$refs["ipport_" + port], portValidation);
    });

    validator.onSuccess((event) => {
      const data = this._getFormData();
      store.updateModuleData("GDLoverUDP", { ...this.copyOfData, ...data }).then(() => {
        this.close();
      });
    });
  }

  _hasValue(item) {
    try {
      return this.$refs[item].value || !this.hasData;
    } catch (e) {
      return false;
    }
  }

  _setFormData(data) {
    this.ports.forEach((portno) => {
      let port = this.$refs["port_" + portno];
      let record = data.defaultPorts[portno];
      if (record) {
        port.value = record;
      } else {
        port.value = "";
      }
    });

    this.ips.forEach((ipno) => {
      let ip = this.$refs["ip_" + ipno];
      let port = this.$refs["ipport_" + ipno];
      let record = data.ips[ipno];
      if (record) {
        ip.value = record.ip;
        port.value = record.port;
      } else {
        ip.value = "";
        port.value = "";
      }
    });
  }

  _getFormData() {
    let ports = Array.from(new Set(this.ports.map((portno) => this.$refs["port_" + portno].value).filter((value) => value)));

    let ips = this.ips
      .map((ipno) => ({
        ip: this.$refs["ip_" + ipno].value,
        port: this.$refs["ipport_" + ipno].value,
      }))
      .filter((value) => value.ip && !value.ip.startsWith("127"));

    return {
      defaultPorts: ports,
      ips: ips,
    };
  }

  render(html) {
    return html`
      <h4>Configuration of the GDL over UDP Decoder</h4>
      <p>
        Flightbags such as SkyDemon, ForeFlight, EasyFVR and others can then determine your location and receive information about other aircraft using this
        protocol.
      </p>

      <form ref="form" autocomplete="off" novalidate="novalidate">
        <div class="section">
          Up to 4 different ports can be configured
          <label class="btn sm btn-medium btn-link p-0 circle mt-n1">
            ${html.raw(icon.help)}
            <p class="tooltip rounded shadow o-90 p-2 bg-dark color-light mw-300 sm outset-bottom inset-left text-left mh-200 overflow-auto">
              Each device connected to GaTas will automatically receive GDL90 packets. By default, these packets are sent on port <b>4000</b>, but up to four
              different ports can be configured to accommodate devices that listen on other ports. Default port is <b>4000</b>
            </p>
          </label>
          <div class="grid md-columns-4 lg-columns-4">
            ${this.ports.map(
              (item) => html`
                <label class="has-sub ${this._hasValue("port_" + item) && "active"}">
                  <sub class="active">Port ${item}</sub>
                  <input type="text" id="port_${item}" ref="port_${item}" onblur="this.parentElement.classList.toggle('active', this.value)" />
                </label>
              `,
            )}
          </div>
        </div>

        <div class="section">
          Up to 4 different IP's can be configured
          <label class="btn sm btn-medium btn-link p-0 circle mt-n1">
            ${html.raw(icon.help)}
            <p class="tooltip rounded shadow o-90 p-2 bg-dark color-light mw-300 sm outset-bottom inset-left text-left mh-200 overflow-auto">
              In addition to ports, up to 4 separate IP addresses can be configured. This is usually only useful if your system connects to an access point.
              Default port is <b>4000</b>
            </p>
          </label>
          <div class="grid md-columns-2 lg-columns-2">
            ${this.ips.map(
              (item) => html`
                <div class="row g-0">
                  <div class="col-10">
                    <label class="has-sub ${this._hasValue("ip_" + item) && "active"}">
                      <sub>IP ${item}</sub>
                      <input type="text" id="ip_${item}" ref="ip_${item}" onblur="this.parentElement.classList.toggle('active', this.value)" />
                    </label>
                  </div>
                  <div class="col-2">
                    <label class="has-sub ${this._hasValue("ipport_" + item) && "active"}">
                      <sub>Port ${item}</sub>
                      <input type="text" id="ipport_${item}" ref="ipport_${item}" onblur="this.parentElement.classList.toggle('active', this.value)" />
                    </label>
                  </div>
                </div>
              `,
            )}
          </div>
        </div>

        ${this.buttonArray(html)}
      </form>
    `;
  }
}
customElements.define("gdloverudp-config", GDLoverUDPConfig);

class Dump1090ClientConfig extends ModuleConfig {
  created() {
    this._initForm(store.getModuleData("Dump1090Client"));
  }

  mounted() {
    const validator = new JustValidate(this.$refs.form);

    validator
      .addField(this.$refs.port, [
        {
          rule: "required",
        },
        ...portValidation,
      ])
      .addField(this.$refs.ip, [
        {
          rule: "required",
          errorMessage: "IPv4 address is required",
        },
        ...ipValidation,
      ])
      .onSuccess((event) => {
        const data = this._getFormData();
        store.updateModuleData("Dump1090Client", { ...this.copyOfData, ...data }).then(() => {
          this.close();
        });
      });
  }

  _setFormData(data) {
    this.$refs.ip.value = data.ip;
    this.$refs.port.value = data.port;
  }

  _getFormData() {
    return {
      ip: this.$refs.ip.value,
      port: this.$refs.port.value,
    };
  }

  render(html) {
    return html`
      <h4>Configuration of the Dump1090 Client</h4>
      <p>
        This module enables reading ADS-B data in the format '*8D7C7181215D01A08208204D8BF1;' from an external system like Dump1090 into GaTas (port
        &lt;IP&gt;:30002), processing them as traffic targets. Ensure that the ADSBDecoder is enabled and there is traffic within the filtered ranges above or
        below so you will actually see them.
          <div class="alert alert-warning">
          ${html.raw(icon.warning)} This modules requires restart after modification of the configuration.
        </div>
      </p>
      <form ref="form" autocomplete="off" novalidate="novalidate">
        <div class="section grid md-columns-2 lg-columns-2">
          <label for="ip">
            IP:
            <input type="text" id="ip" ref="ip" placeholder="SSID" } />
          </label>
          <label for="port">
            Port:
            <input type="text" id="port" ref="port" placeholder="4000" } />
          </label>
        </div>
        <br />

        ${this.buttonArray(html)}
      </form>
    `;
  }
}
customElements.define("dump1090client-config", Dump1090ClientConfig);

class SX1262 extends ModuleConfig {
  created() {
    this._initForm(store.getModuleData(this.name));
  }

  mounted() {
    const validator = new JustValidate(this.$refs.form);

    validator
      .addField(this.$refs.offset, [
        {
          rule: "required",
        },
        {
          rule: "integer",
        },
        {
          rule: "minNumber",
          value: -30000,
        },
        {
          rule: "maxNumber",
          value: 30000,
        },
      ])
      .onSuccess((event) => {
        const data = this._getFormData();
        store.updateModuleData(this.name, { ...this.copyOfData, ...data }).then(() => {
          this.close();
        });
      });
  }

  _setFormData(data) {
    this.$refs.offset.value = data.offset;
    this.$refs.txEnabled.checked = data.txEnabled;
  }

  _getFormData() {
    return {
      offset: this.$refs.offset.value,
      txEnabled: this.$refs.txEnabled.checked,
    };
  }

  render(html) {
    return html`
      <h4>Configuration of the ${this.name} LoraWan module</h4>

      <form ref="form" autocomplete="off" novalidate="novalidate">
        <div class="section grid md-columns-2 lg-columns-2">
          <label for="offset">
            Offset(Hz)
            <label class="btn sm btn-medium btn-link p-0 circle mt-n1">
              ${html.raw(icon.help)}
              <p class="tooltip rounded shadow o-90 p-2 bg-dark color-light mw-300 sm outset-bottom inset-left text-left mh-200 overflow-auto">
                Add's an additional offset to the frequency for the protocol in this device. To calibrate you can use an existing (correctly calibrated) OGN
                receiver and use it's frequency offset. When shown as a negative value,use a positive value in the below input field to offset the frequency.
              </p> </label
            >:
            <input type="text" id="offset" ref="offset" placeholder="0" } />
          </label>
          <label for="txEnabled">
            <label class="btn sm btn-medium btn-link p-0 circle mt-n1">
              TX Enabled ${html.raw(icon.help)}
              <p class="tooltip rounded shadow o-90 p-2 bg-dark color-light mw-300 sm outset-bottom inset-left text-left mh-200 overflow-auto">
                Allow to enable or disable complete transmission of any frames from the radio. Normally you want to have this enabled so others can see you.
              </p> </label
            >:
            <br />
            <input type="checkbox" id="txEnabled" ref="txEnabled" placeholder="1" />
          </label>
        </div>
        <br />

        ${this.buttonArray(html)}
      </form>
    `;
  }
}

class Sx1262_0 extends SX1262 {
  name = "Sx1262_0";
}

class Sx1262_1 extends SX1262 {
  name = "Sx1262_1";
}

customElements.define("sx1262_0-config", Sx1262_0);
customElements.define("sx1262_1-config", Sx1262_1);

class BluetoothConfig extends ModuleConfig {
  created() {
    this._initForm(store.getModuleData("Bluetooth"));
  }

  mounted() {
    const validator = new JustValidate(this.$refs.form);

    validator
      .addField(this.$refs.localName, [
        {
          rule: "required",
        },
        ...ssidValidation,
      ])
      .onSuccess((event) => {
        const data = this._getFormData();
        store.updateModuleData("Bluetooth", { ...this.copyOfData, ...data }).then(() => {
          this.close();
        });
      });
  }

  _setFormData(data) {
    // Get Access Point
    if (data == null || data.localName == null) {
      this.$refs.localName.value = "GaTas";
    } else {
      this.$refs.localName.value = data.localName;
    }
    this.$refs.rfComm.checked = data.rfComm;
  }

  _getFormData() {
    return {
      localName: this.$refs.localName.value,
      rfComm: this.$refs.rfComm.checked,
    };
  }

  render(html) {
    return html`
      <h4>Configuration of the Bluetooth Service</h4>
      <p>
        This module connects allows for a classic bluetooth service and BlueTooth LE:<br />
        Data send will be Dataport, thus NMEA which is the exact same data as when connecting to Air Connect.
      </p>

      <form ref="form" autocomplete="off" novalidate="novalidate">
        <div class="row g-0">
          <label for="localName">
            Device Name:
            <input type="text" id="localName" ref="localName" placeholder="GaTas" />
          </label>
        </div>
        <div class="row g-0">
          <label for="rfComm">
            <input type="checkbox" id="rfComm" id="rfComm" ref="rfComm" placeholder="0" />
            Enable SPP (Serial Port Profile) on Bluetooth. Normally just BLE is enough for systems like SkyDemon.
          </label>
        </div>

        ${this.buttonArray(html)}
      </form>
    `;
  }
}
customElements.define("bluetooth-config", BluetoothConfig);
