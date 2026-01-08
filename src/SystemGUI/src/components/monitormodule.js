import { El } from "@frameable/el";
import store from "./store";
import "./moduleconfigs";

class MonitorModule extends El {
  created() {
    this.state = this.$observable({ data: [] });
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
      .fetch(`/api/${this.selected}.json`)
      .then((data) => {
        this.state.data.length = 0;
        for (var prop in data) {
          this.state.data.push({ name: prop, value: data[prop] });
        }
      })
      .catch((e) => {
        this.state.data.length = 0;
      })
      .finally(() => {
        if (this._running) {
          this.timer = this.timer = setTimeout(() => {
            this._fetchData();
          }, 750);
        }
      });
  }

  _chunkString(str, size, sep = "'") {
    return str.match(new RegExp(`.{1,${size}}`, "g")).join(sep);
  }

  _bitsToDots(str) {
    return str
      .replace(/1/g, "●")
      .replace(/0/g, "·");
  }

  _row(html, item) {
    let value = item.value;

    let isNumeric = false;
    let isBitString = false;

    if (Array.isArray(value)) {
      value = value.join(", ");
    } else if (typeof value === "string" && /^[01]+$/.test(value) && value.length >= 10) {
      isBitString = true;
      value = this._bitsToDots(this._chunkString(value, 10, "|"));
    } else if (typeof value === "number") {
      isNumeric = true;
    }

    // Styling decisions
    let style = "";

    // Monospace only for numbers and bitstrings
    if (isNumeric || isBitString) {
      style += "font-family: monospace;";
    }

    // Error highlighting: name ends with Err AND numeric value > 0
    if (
      typeof item.name === "string" &&
      item.name.endsWith("Err") &&
      typeof item.value === "number" &&
      item.value > 0
    ) {
      style += " color: orange;";
    }

    return html`
    <tr>
      <th style="width:33%" scope="row">${item.name}</th>
      <td style="${style}">${value}</td>
    </tr>
  `;
  }

  _filteredItems() {
    return this.state.data.filter((i) => true);
  }

  render(html) {
    let items = this._filteredItems();
    return html`
      <h4>Monitoring: ${this.selected}</h4>
      <small>
        <table>
          <tbody>
            ${items.map((item) => html` ${this._row(html, item)} `)}
          </tbody>
        </table>
      </small>
    `;
  }
}

customElements.define("monitor-module", MonitorModule);
