import { El } from "@frameable/el";
import store from "./store";

class ActionButtons extends El {
    created() {
        this.state = this.$observable({
            restartDlg: false,
            changeHwDlg: false,
            startApDlg: false,
            apStarted: false
        });
    }

    _restart() {
      store.restart();
      this.state.restartDlg = false;
    }

    _usbBoot() {
      store.usbBoot();
      this.state.usbBootDlg = false;
    }

    _startAp() {
      this.state.apStarted = true;
      store.startAp();
    }

    _restartButton(html) {
        return html`<button class="btn xs" onclick=${() => (this.state.restartDlg = true)}>Restart</button>
          ${this.state.restartDlg ? this._restartAreYouSureDlg(html) : ""} ${this.state.usbBootDlg ? this._usbBootDlgAreYouSureDlg(html) : ""}
    `;
    }

    _usbBootButton(html) {
        return html`<button class="btn xs" onclick=${() => (this.state.usbBootDlg = true)}>Upload Firmware</button>`;
    }

    _restartAreYouSureDlg(html) {
        return html` <div class="modal show">
      <div class="modal-content mw-400 rounded">
        <article class="accent-light shadow">
          <header>
            <h4>Restart GaTas?</h4>
          </header>
          <div class="overflow-auto accent-primary" style="color: black">
            <!-- Quick hack to make text black on Safari Desktop -->
            <p>
              The connection will be temporarily disconnected. Any unsaved data will be available after restart.<br />
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
            <h4>Start Firmware Mode?</h4>
          </header>
          <div class="overflow-auto accent-primary" style="color: black">
            <!-- Quick hack to make text black on Safari Desktop -->
            <p>
              To update GaTas, make sure it is connected to your computer with a USB cable through the <strong>Microcontroller port</strong> (the charge port
              won’t work for this step).
            </p>
            <p>
              When GaTas restarts, it will show up as a new drive on your computer. Once you see the drive, simply drag and drop the
              <strong>GaTas.uf2</strong> file onto it. After a moment, the device will restart automatically, and GaTas will be ready to use again.
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

    _changeHwButton(html) {
        return html`<button class="btn xs" onclick=${() => (this.state.changeHwDlg = true)}>Change Board : ${store.state.hardwareName}</button>
          ${this.state.changeHwDlg === true ? this._changeHardwareDialog(html) : ""}
    `;
    }

    async _hardwareUpdatedConfirm(e) {
        if (this._selectedHwIdx > 0) {
            this.state.changeHwDlg = true;
            await store.updateHardware(this._selectedHwIdx);
            this._selectedHwIdx = 0;
            store.restart();
        }
        this.state.changeHwDlg = false;
    }

    _changeHardwareDialog(html) {
        return html` <div class="modal show">
      <div class="modal-content mw-400 rounded">
        <article class="accent-light shadow">
          <header>
          <h4>Change hardware model?</h4>
          </header>
          <div class="overflow-auto accent-primary" style="color: black">
            <!-- Quick hack to make text black on Safari Desktop -->
            <p>
              This will change the type of board that GaTas is running on. 
              After changing, the connection will be temporarily disconnected.
              Any unsaved data will be available after restart.<br />
            </p>

            <select onchange=${e => this._selectedHwIdx = e.currentTarget.selectedIndex}>
              ${store.availableHardware.map(
            (item) => html`<option ${item.hardware === store.state?.hardware?.type ? "selected" : ""} value="${item.hardware}">${item.name}</option>`,
        )}
            </select>

          </div>
          <footer class="px-2 jc-end">
            <button type="button" class="btn btn-error sm ml-1 md-ml-3" onclick=${this._hardwareUpdatedConfirm}>Change</button>
            <button type="button" class="btn btn-primary sm ml-1 md-ml-3" onclick=${() => (this.state.changeHwDlg = false)}>Cancel</button>
          </footer>
        </article>
      </div>
    </div>`;
    }

    _startApButton(html) {
        return html`<button class="btn xs" onclick=${() => (this.state.startApDlg = true)}>Start Access Point</button>
          ${this.state.startApDlg === true ? this._startApDialog(html) : ""}
    `;
    }

    _startApDialog(html) {
      const footer = !this.state.apStarted?html`
        <footer class="px-2 jc-end">
          <button type="button" class="btn btn-error sm ml-1 md-ml-3" onclick=${this._startAp}>Yes</button>
          <button type="button" class="btn btn-primary sm ml-1 md-ml-3" onclick=${() => (this.state.startApDlg = false)}>No</button>
        </footer>
        `:html`
          <footer>
            <div class="alert alert-primary">
              <p>
              <strong>Access Point mode has been initiated.<br /></strong>
              Please connect to the GA/TAS Access Point to continue the setup.
              </p>
            </div>
          </footer>
        `;

      return html` <div class="modal blur show">
      <div class="modal-content mw-400 rounded">
        <article class="shadow accent-light">
          <header>
            <h4>Start Access Point</h4>
          </header>
          <div class="overflow-auto accent-primary" style="color: black">
            <!-- Quick hack to make text black on Safari Desktop -->           
            <p>
              If GA/TAS is currently in client mode, it will exit that mode and start the GA/TAS Access Point.
            </p>
            <p>
              After GA/TAS restarts, connect your device to the <strong>GATAS</strong> Wi-Fi network. (Or how you named it)
              Then open your browser and go to <strong><a href="http://192.168.1.1">http://192.168.1.1</a></strong> to access the user interface.
              <br /><br />
              Once configured restart the device to get back into normal WIFI mode. 
            </p>
          </div>
          ${footer}
        </article>
      </div>
    </div>`;
    }

    render(html) {
        return html`
      <div style="margin-top: 5px; gap: 1em;align-items: center;justify-content: center;display:flex">
          <div>${this._restartButton(html)}</div>
          <div>${this._usbBootButton(html)}</div>
          <div>${this._changeHwButton(html)}</div>
          <div>${this._startApButton(html)}</div>          
      </div>
    `;
    }
}

customElements.define("action-buttons", ActionButtons);
