import { El } from "@frameable/el";
import store from "./store";
import "./moduleconfigs";
import {formatUnit, formatUnit2, findUnit} from "./units";

const MAX_DISTANCE_COLOR = "rgb(241, 120, 7)";
const AVG_DISTANCE_COLOR = "rgba(23, 208, 23, 1)";


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

        // render polar plots
        for (var prop in data) {
          if (prop.endsWith("AntPolar") && Array.isArray(data[prop])) {
            this._drawPolar(`polar-${prop}`, data[prop]);
          }
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

  _plane(ctx, cx, cy) {
    // draw small airplane at center
    const factor = 2;
    ctx.save();    

    ctx.translate(cx, cy-3);
    ctx.rotate(-Math.PI ); // nose pointing north

    ctx.strokeStyle = "#555";
    ctx.lineWidth = 1.5;

    ctx.beginPath();

    // fuselage
    ctx.moveTo(0, 5*factor);
    ctx.lineTo(0, -6*factor);

    // // wings
    ctx.moveTo(-6*factor, 2*factor);
    ctx.lineTo(6*factor, 2*factor);

    // // tail
    ctx.moveTo(3*factor, -4.5*factor);
    ctx.lineTo(-3*factor, -4.5*factor);

    ctx.stroke();

    ctx.restore();
  }

  _drawGrid(ctx, cx, cy, rMax, n) {

    ctx.strokeStyle = "#999";
    ctx.lineWidth = 1;

    const nCircles = 2;
    for (let i = 1; i <= nCircles; i++) {
      ctx.beginPath();
      ctx.arc(cx, cy, rMax * i / nCircles, 0, Math.PI * 2);
      ctx.stroke();
    }

    const offsetRot = (2 * Math.PI) / n / 2;

    for (let i = 0; i < n; i++) {

      const a = (i / n) * Math.PI * 2 - Math.PI / 2 + offsetRot;

      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(
        cx + Math.cos(a) * rMax,
        cy + Math.sin(a) * rMax
      );
      ctx.stroke();
    }

  }

  _drawDatapoints(ctx, data, index, lineWidth, color, maxDist) {

    const w = ctx.canvas.width;
    const h = ctx.canvas.height;

    const cx = w / 2;
    const cy = h / 2;

    const rMax = Math.min(cx, cy) - 4;

    const n = data.length;

    const points = [];

    for (let i = 0; i < n; i++) {

      const value = data[i][index];

      const a = (i / n) * Math.PI * 2 - Math.PI / 2;
      const r = (value / maxDist) * rMax;

      const x = cx + Math.cos(a) * r;
      const y = cy + Math.sin(a) * r;

      points.push([x, y]);
    }

    ctx.strokeStyle = color;
    ctx.lineWidth = lineWidth;

    ctx.beginPath();
    ctx.moveTo(points[0][0], points[0][1]);

    for (let i = 1; i < points.length; i++) {
      ctx.lineTo(points[i][0], points[i][1]);
    }

    ctx.closePath();
    ctx.stroke();

    ctx.fillStyle = color;

    for (const [x, y] of points) {
      ctx.beginPath();
      ctx.arc(x, y, 3, 0, Math.PI * 2);
      ctx.fill();
    }
  }

  _maxValue(data, idx) {
        let maxDist = 0;

    for (const d of data) {
      if (d[3] > maxDist) { maxDist = d[idx]; }  // use maxDistance scale
    }

    return maxDist;
  }

  _drawPolar(id, data) {

    const canvas = this.$refs[id];
    if (!canvas) { return; }

    const ctx = canvas.getContext("2d");

    const w = canvas.width;
    const h = canvas.height;

    ctx.clearRect(0, 0, w, h);

    const cx = w / 2;
    const cy = h / 2;
    const rMax = Math.min(cx, cy) - 4;

    const n = data.length;

    let maxDist = this._maxValue(data, 3);
    this._drawGrid(ctx, cx, cy, rMax, n);
    this._plane(ctx, cx, cy);

    this._drawDatapoints(ctx, data, 3, 2, MAX_DISTANCE_COLOR, maxDist);     // maxDistance
    this._drawDatapoints(ctx, data, 2, 1, AVG_DISTANCE_COLOR, maxDist); // avgDistance
  }


  _renderDefault(html, item) {
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

    let style = "";
    if (isNumeric || isBitString) {
      style += "font-family: monospace;";
    }

    if (
      typeof item.name === "string" &&
      item.name.endsWith(":err") &&
      typeof item.value === "number" &&
      item.value > 0
    ) {
      style += " color: orange;";
    }

    return { value, style };
  }

  _row(html, item) {
    if (item.name.endsWith("AntPolar") && Array.isArray(item.value)) {
      const id = `polar-${item.name}`;

      return html`
      <tr>
        <th style="width:33%" scope="row">${item.name}</th>
        <td>
          <div style="display:flex; align-items:center; gap:10px">
            <canvas ref="${id}" width="120" height="120"></canvas>

            <div style="font-size:11px; line-height:1.4">
              <div>
                <span style="display:inline-block;width:12px;height:3px;background:${AVG_DISTANCE_COLOR};margin-right:6px"></span>
                Avg Distance 
              </div>

              <div>
                <span style="display:inline-block;width:12px;height:3px;background:${MAX_DISTANCE_COLOR};margin-right:6px"></span>
                Max Distance
              </div>
            </div>
          </div>
        </td>
      </tr>
    `;
    }

    const rendered = this._renderDefault(html, item);
    const result = formatUnit2(item.name, rendered.value)

    return html`
    <tr>
      <th style="width:33%" scope="row">${result.name}</th>
      <td style="${rendered.style}">${result.value}</td>
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
