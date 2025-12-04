import { El } from "@frameable/el";
import store from "./store";
import "./aircraftsession";
import "./aircraftconfig";

const MAX_AIRCRAFT = 6;
class Session extends El {
  created() {
    this.state = this.$observable({
      showHelp: false,
      editAircraft: false,
      aircraft: "",
      deleteAircraftDlg: false,
    });
  }

  render(html) {
    return this.state.editAircraft ? this._editAircraft(html) : this._selection(html);
  }

  _editAircraft(html) {
    return html` <aircraft-config key="config" close=${() => (this.state.editAircraft = false)} selected="${this.state.aircraft}"> </aircraft-config> `;
  }

  _aircraftUpdated(aircraftId) {
    store.setDefaultAirCraftId(aircraftId);
  }

  _add() {
    this.state.aircraft = "";
    this.state.editAircraft = true;
  }
  _edit() {
    this.state.aircraft = store.state.aircraftId;
    this.state.editAircraft = true;
  }

  _deleteAircraft() {
    return store
      .deleteAircraft(store.state.aircraftId)
      .catch((e) => {
        this.state.data.length = 0;
      })
      .finally(() => {
        this.state.deleteAircraftDlg = false;
      });
  }

  _selection(html) {
    var deleteDlg = this.state.deleteAircraftDlg ? this._deleteAircraftDlg(html) : "";
    return html`
      ${deleteDlg}
      <aircraft-session key="session" selected="${store.state.aircraftId}" changed=${this._aircraftUpdated}></aircraft-session>

      <div class="section grid md-columns-3 lg-columns-3">
        <input
          type="submit"
          class="btn"
          value="Add ${store.state.numberOfAircrafts >= MAX_AIRCRAFT ? `(Max ${MAX_AIRCRAFT})` : ""}"
          onclick=${this._add}
          ${store.state.numberOfAircrafts >= MAX_AIRCRAFT ? "disabled" : ""}
        />
        <input
          type="submit"
          class="btn"
          value="Remove"
          onclick=${() => (this.state.deleteAircraftDlg = true)}
          ${store.state.numberOfAircrafts < 2 ? "disabled" : ""}
        />
        <input type="submit" value="Modify" class="btn btn-primary" onclick=${this._edit} />
      </div>
    `;
  }

  _deleteAircraftDlg(html) {
    return html` <div id="large-modal" class="modal show">
      <div class="modal-content rounded lg-mw">
        <article class="accent-light shadow">
          <header>
            <h4>Delete '${store.state.aircraftId}' ?</h4>
          </header>
          <div class="overflow-auto">
            <p>Removal of <b>${store.state.aircraftId}</b> cannot be undone.<br />Are you sure you want to delete ${store.state.aircraftId} aircraft?</p>
          </div>
          <footer class="px-2 jc-end">
            <button type="button" class="btn btn-warning sm ml-1 md-ml-3" onclick=${this._deleteAircraft}>Yes</button>
            <button type="button" class="btn btn-primary sm ml-1 md-ml-3" onclick=${() => (this.state.deleteAircraftDlg = false)}>No</button>
          </footer>
        </article>
      </div>
    </div>`;
  }
}

customElements.define("gatas-session", Session);
