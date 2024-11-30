import { El } from "@frameable/el";
import store from "./store";
import JustValidate from "just-validate";

class AircraftConfig extends El {
  created() {
    this.types = [
      "Glider",
      "GliderMotorGlider",
      "TowPlane",
      "Helicopter",
      "Skydiver",
      "DropPlane",
      "HangGlider",
      "Paraglider",
      "ReciprocatingEngine",
      "JetTurbopropEngine",
      "Balloon",
      "Airship",
      "Uav",
      "StaticObstacle",
    ];
    this.transponderTypes = ["ICAO", "FLARM", "OGN", "ADSL"];
    this.protocolTypes = ["FLARM", "OGN", "ADSL"];

    this.state = this.$observable({ showHelp: false, aircraft: {} });
    this.copyOfAircraft = {};
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
    this.$refs.privacy.checked = aircraft.privacy;
    for (let i of this.protocolTypes) {
      this.$refs[`protocol${i.toUpperCase()}`].checked = aircraft.protocols.includes(i);
    }
  }

  _getFormData() {
    let aircraft = {};
    aircraft.callSign = this.$refs.callSign.value.toUpperCase();
    aircraft.category = this.$refs.category.value;
    aircraft.addressType = this.$refs.transponder.value;
    aircraft.address = Number("0x" + this.$refs.address.value);
    aircraft.noTrack = this.$refs.noTrack.checked;
    aircraft.privacy = this.$refs.privacy.checked;
    aircraft.protocols = [];
    for (let i of this.protocolTypes) {
      if (this.$refs[`protocol${i.toUpperCase()}`].checked) {
        aircraft.protocols.push(i);
      }
    }
    return aircraft;
  }

  _fetchData() {
    if (this.selected) {
      return store.fetch(`/api/_Configuration/aircraft/${this.selected}.json`).catch((e) => {
        Object.assign(this.state.aircraft, {});
      });
    } else {
      return Promise.resolve({
        category: "ReciprocatingEngine",
        addressType: "ICAO",
        protocols: ["OGN", "ADSL"],
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
          <select ref="category" required>
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
            <h6>Protocols to Enable (Send and Receive)</h5>
            
            ${this.protocolTypes.map(
              (item) => html`
                <label>
                  <input type="checkbox" ref="protocol${item.toUpperCase()}" />
                  ${item}
                </label>
              `,
            )}

        </div>
        <div>
          <h6>Other</h5>
          <label for="privacy">
            <input type="checkbox" id="privacy" ref="privacy" />Privacy
          </label>
          <small>OpenAce Will use an random address and send it for the duration of the session.</small>

          <label for="noTrack">
            <input type="checkbox" id="noTrack" ref="noTrack" />No Track
          </label>
            <small>OpenAce will indicate to other receiver that you don't want to be tracked.</small>  
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
                  <li>If you have an mode-s transponder, use that code, select ADSL under <i>Your official Transponder</i>
                  <li>If you only have an FLARM transponder, use the code from that device, select FLARM under <i>Your official Transponder</i>
                  <li>For OGN/ADSL transponders use the code from these devices, select OGN/ADSL under <i>Your official Transponder</i>
                </ul>
                From the above protocols, only enable the protocols of the devices you don't own yet to
                avoid duplicate transmissions on the same protocol.
            </p>

            <p>            
            <b>Protocols to Enable</b><br />
            Enable the protocols that are used to send/receive aircraft information on. To enable ADS-B use the correct module under 'modules'.
            </P>
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
