import { El } from "@frameable/el";
import store from "./store";

class Menu extends El {
  _setPage(page) {
    store.state.page = page;
  }

  _saveBr() {
    store.storeInBRModuleData();
  }

  render(html) {
    return html`
      <div class="bg-light flex ai-center jc-between relative toolbar px-1">
        <h5 class="flex-center">
          <a href="#session" onclick=${() => this._setPage("session")}>GA/TAS :: ${store.state.aircraftId}</a>
        </h5>
        ${store.state.connected ? html`<div class="alert-success">GA/TAS Connected</div>` : html`<div class="alert-error">GA/TAS Disconnected</div>`}
        <button class="btn xs ${store.state.configModified ? "btn-warning" : ""}" onclick=${() => this._saveBr()}>Save</button>
        <div>
          <label class="toggle btn btn-link p-0 btn-menu md-none">
            <input type="checkbox" />
          </label>
          <div class="md-open">
            <nav class="bg-light inset-right md-horizontal md-hoverable">
              <ul>
                <li>
                  <a href="#modules" onclick=${() => this._setPage("modules")}>Modules</a>
                </li>
                <li>
                  <a href="#status" onclick=${() => this._setPage("status")}>Status</a>
                </li>
              </ul>
            </nav>
          </div>
        </div>
      </div>
    `;
  }
}

customElements.define("main-menu", Menu);
