import { El } from "@frameable/el";
import store from "./store";

class AirCraftSession extends El {
  created() {
    this.state = this.$observable({ aircraft: [] });
  }

  _aircraftUpdated(e) {
    let idx = e.currentTarget.selectedIndex;
    let aircraftId = store.state.aircrafts[idx].callSign;
    if (aircraftId === store.state.aircraftId) return;
    // Call the assigned function to update the aircraft
    if (aircraftId) {
      this.changed(aircraftId);
    }
  }

  render(html) {
    return html`
      <div class="section">
        <label>
          Aircraft:
          <select onchange=${this._aircraftUpdated}>
            ${store.state.aircrafts.map(
              (item) => html`<option ${item.callSign === this.selected ? "selected" : ""} value="${item.callSign}">${item.callSign}</option>`,
            )}
          </select>
        </label>
      </div>
    `;
  }
}

customElements.define("aircraft-session", AirCraftSession);
