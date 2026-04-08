import { El } from "@frameable/el";
import store from "./store";
import JustValidate from "just-validate";

class AircraftConfig extends El {
  created() {
    this.types = [
      "Light",
      "Small",
      "Large",
      "Aerobatic",
      "Helicopter",
      "Glider",
      "Balloon",
      "Sky Diver",
      "Ultra Light",
      "Unmanned aerial vehicle",
      "Surface Emergency Vehicle",
      "Surface Vehicle",
      "Point Obstacle",
      "Gyrocopter",
      "Hang Glider",
      "Para Glider",
      "Drop Plane"];

    this.transponderTypes = ["ICAO", "FLARM", "OGN", "ADSL"];
    this.protocolTypes = ["OGN", "FLARM", "ADSL", "FANET"];
    this.protocolModes = [
      { value: "OFF", label: "OFF" },
      { value: "RX_TX", label: "RX/TX" },
      { value: "RX", label: "RX" },
      { value: "TX", label: "TX" },
    ];

    this.state = this.$observable({ showHelp: false, aircraft: {}, groundStation: false });
    this.copyOfAircraft = {};
    this.protocolModeState = {};
    this._resetProtocolModeState();
    this._fetchData().then((data) => {
      Object.assign(this.state.aircraft, data);
      Object.assign(this.copyOfAircraft, data);
      this._setFormData(this.state.aircraft);
    });
  }

  mounted() {
    const validator = new JustValidate(this.$refs.form);

    validator
      .addField(this.$refs.callSign, [
        {
          rule: "required",
        },
        {
          rule: "customRegexp",
          value: /^[A-Za-z0-9\-]*$/,
        },
        {
          rule: "minLength",
          value: 3,
        },
        {
          rule: "maxLength",
          value: 7,
        },
      ])
      .addField(this.$refs.address, [
        {
          rule: "required",
        },
        {
          rule: "customRegexp",
          value: /^[a-zA-Z0-9\-]*$/,
        },
        {
          rule: "minLength",
          value: 6,
        },
        {
          rule: "maxLength",
          value: 6,
        },
      ])
      .addField(this.$refs.heightAboveGps, [
        {
          rule: "minNumber",
          value: 0,
        },
        {
          rule: "maxNumber",
          value: 1500,
        },
      ])
      .onSuccess((event) => {
        const aircraft = this._getFormData();
        this._updateData(aircraft).then(() => {
          store.setDefaultAirCraftId(aircraft.callSign).then((data) => {
            this.close();
          });
        });
      });
  }

  _addressFormat(number) {
    number = number & 0xffffff;
    return number !== undefined ? number.toString(16).toUpperCase().padStart(6, "0") : "000000";
  }

  _onGroundStationChange(e) {
    this._updateGroundStationState(e.target.checked);
  }

  _resetProtocolModeState() {
    this.protocolModeState = Object.fromEntries(this.protocolTypes.map((protocol) => [protocol, "OFF"]));
  }

  _protocolRefName(protocol, mode) {
    return `protocol${protocol.toUpperCase()}${mode}`;
  }

  _protocolInputId(protocol, mode) {
    return `protocol_${protocol.toUpperCase()}_${mode}`;
  }

  _protocolLabelRefName(protocol, mode) {
    return `protocolLabel${protocol.toUpperCase()}${mode}`;
  }

  _protocolModeLabelStyle(active) {
    const base = [
      "display: inline-flex",
      "align-items: center",
      "gap: 0.35rem",
      "margin: 0",
      "padding: 0.4rem 0.75rem",
      "border-radius: 999px",
      "border: 1px solid #7b8794",
      "font: inherit",
      "outline: none",
      "appearance: none",
      "cursor: pointer",
      "user-select: none",
      "transition: background-color 120ms ease, color 120ms ease, border-color 120ms ease",
      "background: #f5f7fa",
      "color: #1f2937",
    ];
    if (active) {
      base.push("background: #203040");
      base.push("color: #ffffff");
      base.push("border-color: #203040");
    }
    return base.join("; ");
  }

  _setProtocolMode(protocol, mode) {
    this.protocolModeState[protocol] = mode;
    this._updateProtocolModeStyles(protocol);
  }

  _onProtocolModeChange(protocol, mode) {
    this._setProtocolMode(protocol, mode);
  }

  _updateProtocolModeStyles(protocol) {
    for (const mode of this.protocolModes) {
      const labelRef = this.$refs[this._protocolLabelRefName(protocol, mode.value)];
      if (labelRef) {
        const active = this.protocolModeState[protocol] === mode.value;
        labelRef.style.background = active ? "#203040" : "#f5f7fa";
        labelRef.style.color = active ? "#ffffff" : "#1f2937";
        labelRef.style.borderColor = active ? "#203040" : "#7b8794";
      }
    }
  }

  _normalizeProtocolModes(protocols) {
    const normalized = Object.fromEntries(this.protocolTypes.map((protocol) => [protocol, "OFF"]));
    if (!Array.isArray(protocols)) {
      return normalized;
    }

    for (const protocol of protocols) {
      if (typeof protocol === "string") {
        normalized[protocol] = "RX_TX";
        continue;
      }

      if (protocol && typeof protocol === "object") {
        for (const [protocolName, protocolConfig] of Object.entries(protocol)) {
          if (!this.protocolTypes.includes(protocolName)) {
            continue;
          }

          const mode = protocolConfig?.mode ?? "RX_TX";
          normalized[protocolName] = this.protocolModes.some((item) => item.value === mode) ? mode : "RX_TX";
        }
      }
    }

    return normalized;
  }

  _updateGroundStationState(enabled) {
    this.state.groundStation = enabled;
    if (enabled) {
      this.$refs.category.value = "Point Obstacle";
    }
  }

  _resetForm() {
    Object.assign(this.state.aircraft, this.copyOfAircraft);
    this._setFormData(this.state.aircraft);
  }

  _setFormData(aircraft) {
    this.$refs.callSign.value = aircraft.callSign?.toUpperCase() ?? "";
    this.$refs[`category${aircraft.category?.toUpperCase()}`].selected = true;
    this.$refs[`transponderType${aircraft.addressType?.toUpperCase()}`].selected = true;
    this.$refs.address.value = this._addressFormat(aircraft.address).toUpperCase();
    this.$refs.noTrack.checked = aircraft.noTrack;
    this.$refs.groundStation.checked = aircraft.groundStation;
    this._updateGroundStationState(!!aircraft.groundStation);
    this.$refs.heightAboveGps.value = aircraft.heightAboveGps ?? 0;
    this.protocolModeState = this._normalizeProtocolModes(aircraft.protocols);
    this.protocolTypes.forEach((protocol) => this._updateProtocolModeStyles(protocol));
    //    this.$refs.autoConf.checked = aircraft.autoConf;
    //    this.$refs.privacy.checked = aircraft.privacy;
  }

  _getFormData() {
    let aircraft = {};
    aircraft.callSign = this.$refs.callSign.value.toUpperCase();
    aircraft.category = this.$refs.category.value;
    aircraft.addressType = this.$refs.transponder.value;
    aircraft.address = Number("0x" + this.$refs.address.value);
    aircraft.noTrack = this.$refs.noTrack.checked;
    aircraft.groundStation = this.$refs.groundStation.checked;
    aircraft.heightAboveGps = aircraft.groundStation ? (Number(this.$refs.heightAboveGps.value) || 0) : 0;
    //    aircraft.autoConf = this.$refs.autoConf.checked;

    //    aircraft.privacy = this.$refs.privacy.checked;
    aircraft.protocols = [];
    for (let protocol of this.protocolTypes) {
      const mode = this.protocolModeState[protocol] ?? "OFF";
      if (mode !== "OFF") {
        aircraft.protocols.push({
          [protocol]: {
            mode,
          },
        });
      }
    }
    return aircraft;
  }

  _fetchData() {
    if (this.selected) {
      return store.fetch(`/api/Config/aircraft/${this.selected}.json`).catch((e) => {
        Object.assign(this.state.aircraft, {});
      });
    } else {
      return Promise.resolve({
        category: "Light",
        addressType: "ICAO",
        protocols: [
          { OGN: { mode: "RX_TX" } },
          { ADSL: { mode: "RX_TX" } },
        ],
      });
    }
  }

  _updateData(aircraft) {
    return store.updateAircraft(aircraft).catch((e) => {
      Object.assign(this.state.aircraft, {});
    });
  }

  render(html) {
    let help = this.state.showHelp ? this._help(html) : "";
    return html`
    <form ref="form" autocomplete="off" novalidate="novalidate">
      <div class="section grid md-columns-2 lg-columns-2">
        <label for="callsign">
          Call Sign:
          <input type="text" id="callsign" ref="callSign" placeholder="Call Sign" ${this.selected ? "disabled" : ""}/>
        </label>
        <label>
          Aircraft Type:
          <select ref="category" required ${this.state.groundStation ? "disabled" : ""}>
          ${this.types.map((item) => html` <option ref="category${item.toUpperCase()}">${item}</option>`)}
          </select>
        </label>
        <label>
          Your official Transponder:
          <select ref="transponder" required>
          ${this.transponderTypes.map((item) => html` <option ref="transponderType${item.toUpperCase()}">${item}</option> `)}
          </select>
        </label>
        <label>
          Transponder Code:
          <input type="text" id="address" ref="address" placeholder="000000" />
        </label>
      </div>

      <div class="section row g-3">
        <div>
            <h6>Protocol Modes</h6>

            ${this.protocolTypes.map(
              (item) => html`
                <div class="mb-2" style="display: flex; flex-wrap: wrap; gap: 0.75rem; align-items: center;">
                  <div class="fw-bold" style="min-width: 5rem;">${item}</div>
                  <div style="display: flex; flex-wrap: wrap; gap: 0.75rem; align-items: center;">
                    ${this.protocolModes.map(
                      (mode) => html`
                        <label ref="${this._protocolLabelRefName(item, mode.value)}" for="${this._protocolInputId(item, mode.value)}" style="${this._protocolModeLabelStyle(this.protocolModeState[item] === mode.value)}">
                          <input
                            type="radio"
                            id="${this._protocolInputId(item, mode.value)}"
                            name="protocol${item.toUpperCase()}"
                            ref="${this._protocolRefName(item, mode.value)}"
                            value="${mode.value}"
                            ${this.protocolModeState[item] === mode.value ? "checked" : ""}
                            onchange=${() => this._onProtocolModeChange(item, mode.value)}
                            style="margin: 0; accent-color: currentColor;"
                          />
                          <span>${mode.label}</span>
                        </label>
                      `,
                    )}
                  </div>
                </div>
              `,
            )}

        </div>
        <div>

          <!-- <h5>Other</h5>
          <label for="privacy">
            <input type="checkbox" id="privacy" ref="privacy" />Privacy
          </label>
          <small>GaTas Will use an random address and send it for the duration of the session.</small> -->

          <!--
          <label for="autoConf">
            <input type="checkbox" id="autoConf" ref="autoConf" />Auto Configure
          </label>
          <small>When enabled, GATAS will automatically treat this aircraft as yours if it detects a mismatched transponder code via ADS-B or MLAT, based on matching location and movement. It will then reconfigure GATAS accordingly. </small>
          -->

          <label for="noTrack">
            <input type="checkbox" id="noTrack" ref="noTrack" />No Track
          </label>
          <small>GaTas will indicate to other receivers that you don't want to be tracked.</small>
        </div>
      </div>
      <hr />
      <div>
        <label for="groundStation">
        <input type="checkbox" id="groundStation" ref="groundStation" onchange=${this._onGroundStationChange} />Ground Station
        </label>
        <small>When enabled, this devices acts as a ground station.<br /> Most importantly, it will send additional traffic using ADSL Uplink to other aircraft.</small>
        <div style="display: ${this.state.groundStation ? 'block' : 'none'}">
          <label for="heightAboveGps">
            Height above GaTas (m):
            <input type="number" id="heightAboveGps" ref="heightAboveGps" placeholder="0" />
          </label>
          <small>Height of the static object above the GaTas device in meters.</small>
        </div>
      </div>

      <!-- Buttons -->
      <div class="section grid md-columns-4 lg-columns-4">
        <input class="btn" value="Help" onclick=${() => (this.state.showHelp = true)} />
        <input class="btn" value="Cancel" onclick=${this.close} />
        <input class="btn" value="Reset" onclick=${this._resetForm} />
        <input class="btn btn-primary" type="submit" value="Save" />
      </div>
    </form>

    ${help}
    `;
  }

  _help(html) {
    return html`
    <div id="large-modal" class="modal ${this.state.showHelp ? "show" : ""}">
      <div class="modal-content rounded lg-mw">
        <article class="accent-light shadow">
          <header>
              <h4>Help</h4>
          </header>
          <div class="mh-500 overflow-auto">
              <p>
                <b>Your official Transponder</b><br />
                <ul>
                  <li>If you have a mode-s transponder, use that code, select ADSL under <i>Your official Transponder</i>.
                  <li>If you only have a FLARM transponder, use the code from that device, select FLARM under <i>Your official Transponder</i>.
                  <li>For OGN/ADSL transponders use the code from these devices, select OGN/ADSL under <i>Your official Transponder</i>.
                </ul>
                From the above protocols, only enable the protocols of the devices you don't own yet to
                avoid duplicate transmissions on the same protocol.
            </p>

            <p>
            <b>Protocol Modes</b><br />
            Select whether each protocol should be disabled, receive only, transmit only, or both receive and transmit. To enable ADS-B use the correct module under 'Modules'.
            </p>

            <p>
              <b>Ground station:</b><br />
              <ul>
                <li> it transmits the closest 10 aircraft pver ADS-L O-band
                <li> Allows you to set a altitde of the static object, this indicating surrounding traffic something is here. Ideal for (hang) glider fields
                <li> Transponder will be forced to be ADS-L
              </ul>
            </p>
          </div>
          <footer class="px-2 jc-end">
              <button type="button" class="btn btn-primary sm ml-1 md-ml-3" onclick=${() => (this.state.showHelp = false)}>Close</button>
          </footer>
        </article>
      </div>
    </div>`;
  }
}

customElements.define("aircraft-config", AircraftConfig);
