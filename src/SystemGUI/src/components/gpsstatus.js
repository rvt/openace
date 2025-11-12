import { El } from "@frameable/el";
import store from "./store";

class GpsStatus extends El {
  created() {
    this.state = this.$observable({ data: {}, counter: 0 });
  }

  mounted() {
    this._running = true;
    this._fetchData();
  }

  unmounted() {
    this._running = false;
    clearTimeout(this.timer);
  }

  _fetchData() {
    store
      .fetch("/api/GpsDecoder.json")
      .then((data) => {
        Object.assign(this.state.data, data);
      })
      .catch((e) => {
        Object.assign(this.state.data, {});
      })
      .finally(() => {
        if (this._running) {
          this.timer = setTimeout(() => {
            this._fetchData();
          }, 1000);
        }
      });
  }

  _row(html, label, value, unit) {
    return html`
      <tr>
        <th style="width:50%" scope="row">${label}</th>
        <td>${value} ${unit}</td>
      </tr>
    `;
  }

  render(html) {
    return html`
      <div class="section">
        <table>
          <tbody>
            ${this._row(html, "Time (UTC)", this.state.data?.UtcTimeMsg)} 
            ${this._row(html, "Satellites used in fix", this.state.data?.satsUsedForFix)}
            ${this._row(html, "Satellites in view", this.state.data?.satsInView)}
            ${this._row(html, "Longitude", this.state.data?.longitude)}
            ${this._row(html, "Latitude", this.state.data?.latitude)}
            ${this._row(html, "Geoid Separation", (this.state.data?.geoidSeparation * 3.281).toFixed(0), "ft")}
            ${this._row(html, "WGS84 Ellipsoid", (this.state.data?.altitudeGeoid * 3.281).toFixed(0), "ft")}
            ${this._row(html, "Groundspeed", (this.state.data?.groundspeed * 1.94).toFixed(0), "knt")}
            ${this._row(html, "Track", this.state.data?.track?.toFixed(0), "deg")}
            ${this._row(html, "pDOP", this.state.data?.pDop + " / " + this.state.data?.dopValue)} 
            ${this._row(html, "Fix Quality", this.state.data?.fixQuality)}
            ${this._row(html, "GGA Messages", this.state.data?.receivedGGA)}
            ${this._row(html, "GaTas build", this.state.data?.GaTas_buildTime)}
          </tbody>
        </table>
      </div>
    `;
  }
}

customElements.define("gps-status", GpsStatus);
