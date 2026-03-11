import { El } from "@frameable/el";
import store from "./store";
import {formatUnit, findUnit} from "./units";

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

  _row(html, label, value) {
    return html`
      <tr>
        <th style="width:50%" scope="row">${label}</th>
        <td>${value}</td>
      </tr>
    `;
  }

  render(html) {

    const groundspeed = formatUnit(this.state.data['groundspeed:kt'], "kt");
    const geoidSeparation = formatUnit(this.state.data['geoidSeparation:ft'], "ft");
    const altitudeGeoid = formatUnit(this.state.data['altitudeGeoid:ft'], "ft");
    const track = formatUnit(this.state.data['track:deg'], "deg");
    const receivedGGA = formatUnit(this.state.data['receivedGGA:k'], "k");

    return html`
      <div class="section">
        <table>
          <tbody>
            ${this._row(html, "Time (UTC)", this.state.data?.UtcTimeMsg)} 
            ${this._row(html, "Satellites used in fix", this.state.data?.satsUsedForFix)}
            ${this._row(html, "Satellites in view", this.state.data?.satsInView)}
            ${this._row(html, "Longitude", this.state.data?.longitude)}
            ${this._row(html, "Latitude", this.state.data?.latitude)}
            ${this._row(html, "Geoid Separation", geoidSeparation)}
            ${this._row(html, "WGS84 Ellipsoid", altitudeGeoid)}
            ${this._row(html, "Groundspeed", groundspeed)}
            ${this._row(html, "Track", track)}
            ${this._row(html, "pDOP", this.state.data?.pDop + " / " + this.state.data?.dopValue)} 
            ${this._row(html, "Fix Quality", this.state.data?.fixQuality)}
            ${this._row(html, "Fix Type", this.state.data?.gpsFixType)}
            ${this._row(html, "GGA Messages", receivedGGA)}
            ${this._row(html, "GA/TAS Build Time", this.state.data?.GATAS_BUILD_TIMESTAMP)}
            ${this._row(html, "GA/TAS Version", this.state.data?.GATAS_BUILD_GIT_TAG)}
          </tbody>
        </table>
      </div>
    `;
  }
}

customElements.define("gps-status", GpsStatus);
