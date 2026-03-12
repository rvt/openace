import { El } from "@frameable/el";
import store from "./store";
import "./moduleconfigs";
import { isDarkMode } from "./utils";
import { formatUnit, formatUnit2 } from "./units";

const MAX_DISTANCE_IDX = 3;
const AVG_DISTANCE_IDX = 2;
const PLANE_PATH = new Path2D('m 15.388371,4.7812376 c 0.06737,0.067371 0.06088,0.1535326 -0.171754,0.656096 -0.02746,0.059318 -0.18034,0.2765235 -0.18034,0.2765235 -10e-7,-1e-6 0.102687,0.1129918 0.130532,0.1408372 0.05383,0.053834 0.07864,0.1746392 0.05668,0.2696526 -0.06814,0.2947833 -0.8899,1.4704243 -1.349979,1.9305048 -0.285512,0.2855112 -0.432705,0.4805551 -0.422513,0.5599149 0.0086,0.06697 0.116774,0.3550941 0.240455,0.6389223 0.218228,0.5008044 0.299971,0.5993204 2.76179,3.3388821 1.949531,2.169479 2.546055,2.86956 2.58145,3.028007 0.09814,0.43933 -0.282015,0.847468 -1.264103,1.35685 l -0.506673,0.262782 c 0,0 -7.3888187,-5.289995 -7.3888187,-5.289995 l -4.429513,3.364643 0.101334,0.18034 c 0.055981,0.09885 0.518862,0.676109 1.028802,1.282996 0.509938,0.606889 0.932924,1.160835 0.939489,1.231471 0.026392,0.283961 -1.110644,1.177107 -1.281278,1.006472 -0.269318,-0.269317 -1.398977,-1.131169 -1.494252,-1.14044 -0.068024,-0.0066 -1.039054,-0.747073 -1.368872,-1.076892 -0.329819,-0.329818 -1.070274,-1.300849 -1.076892,-1.368872 -0.00927,-0.09528 -0.871124,-1.224934 -1.140441,-1.494253 -0.17063503,-0.170635 0.722512,-1.30767 1.006474,-1.281277 0.070635,0.0066 0.624579,0.42955 1.231469,0.939488 0.606887,0.50994 1.184148,0.972821 1.282997,1.028802 l 0.180339,0.101334 3.364644,-4.429513 c 0,0 -5.289996,-7.3888202 -5.289997,-7.3888202 l 0.262784,-0.506672 c 0.509382,-0.9820888 0.917519,-1.3622407 1.356849,-1.2641029 0.158448,0.035395 0.858528,0.6319166 3.028007,2.5814488 2.7395627,2.4618182 2.8380777,2.5435617 3.3388797,2.7617902 0.283829,0.1236801 0.571952,0.2318519 0.638923,0.2404552 0.07936,0.010191 0.274402,-0.1370033 0.559915,-0.4225135 0.460079,-0.4600805 1.635721,-1.2818435 1.930505,-1.3499793 0.09501,-0.021963 0.215817,0.00284 0.269652,0.056678 0.02785,0.027846 0.139121,0.1288154 0.139121,0.1288154 0,0 0.217204,-0.1528832 0.276522,-0.1803404 0.502564,-0.2326341 0.590442,-0.2374085 0.657815,-0.1700356 z')

const POLAR_COLORS = {
  DARK: {
    grid: "#666",
    legend: "#999",
    planeFill: "#999",
    avgDistance: "rgb(241, 120, 7)",
    maxDistance: "rgba(23, 208, 23, 1)",
  },
  LIGHT: {
    grid: "#CCC",
    legend: "#333",
    planeFill: "#333",
    avgDistance: "rgb(241, 120, 7)",
    maxDistance: "rgba(23, 208, 23, 1)",
  }
}


class MonitorModule extends El {

  created() {
    this.state = this.$observable({ data: [] });
  }

  mounted() {
    this._running = true;
    debugger
    this._colorSchema = this._getPolarColorSchema();
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

  _getPolarColorSchema() {
    return POLAR_COLORS[isDarkMode() ? "DARK" : "LIGHT"];
  }

  /**
   * 
   * @param Take a string of 0 and one, and replace them with big and small dots 
   * @returns 
   */
  _bitsToDots(str) {
    return str
      .replace(/1/g, "●")
      .replace(/0/g, "·");
  }

  /**
   * Draw a small 2D plane
   * 
   * @param {*} ctx 
   * @param {*} cx 
   * @param {*} cy 
   */
  _plane(ctx, cx, cy) {
    const WX = 10;
    const WY = 10;
    const SCALE = 1.5;

    ctx.save();
    ctx.lineWidth = 1;
    ctx.fillStyle = this._colorSchema.planeFill;
    ctx.translate(cx, cy);
    ctx.rotate(-Math.PI / 4); // nose pointing north
    ctx.translate(-WY * SCALE, -WY * SCALE);
    ctx.scale(SCALE, SCALE);
    ctx.fill(PLANE_PATH);
    ctx.restore();
  }

  /**
   * Draw the spokes
   * @param {*} ctx 
   * @param {*} cx 
   * @param {*} cy 
   * @param {*} rMax 
   * @param {*} n 
   * @param {*} legend 
   */
  _drawGrid(ctx, cx, cy, rMax, n, legend) {
    ctx.save();
    ctx.strokeStyle = this._colorSchema.grid;
    ctx.lineWidth = 1;

    // Arcs
    const nCircles = 2;
    for (let i = 1; i <= nCircles; i++) {
      ctx.beginPath();
      ctx.arc(cx, cy, rMax * i / nCircles, 0, Math.PI * 2);
      ctx.stroke();
    }

    // Spokes
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

    // ---- label at top of outer circle ----
    ctx.fillStyle = this._colorSchema.legend;
    ctx.font = "12px sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "bottom";
    ctx.fillText(legend, cx, 12);
    ctx.restore();
  }

  /**
   * Draw the polar diagram
   * @param {*} ctx 
   * @param {*} data 
   * @param {*} index 
   * @param {*} lineWidth 
   * @param {*} color 
   * @param {*} maxPolar 
   */
  _drawDatapoints(ctx, data, index, lineWidth, color, maxPolar) {
    ctx.save();
    const w = ctx.canvas.width;
    const h = ctx.canvas.height;
    const cx = w / 2;
    const cy = h / 2;

    // Maximum radius
    const rMax = Math.min(cx, cy) - 4;

    const n = data.length;
    const points = [];
    for (let i = 0; i < n; i++) {
      const value = data[i][index];
      const a = (i / n) * Math.PI * 2 - Math.PI / 2;
      const r = (value / maxPolar) * rMax;
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
    ctx.restore();
  }

  _maxValue(data, idx) {
    let maxPolar = 0;
    for (const d of data) {
      if (d[MAX_DISTANCE_IDX] > maxPolar) { maxPolar = d[idx]; }  // use maxDistance scale
    }

    return maxPolar;
  }

  _pickDistance(max) {
    const MAX_DISTANCES = [100, 250, 1000, 5000, 10000, 20000, 25000, 50000];
    for (const d of MAX_DISTANCES) {
      if (max <= d) {
        return d;
      }
    }
    return MAX_DISTANCES[MAX_DISTANCES.length - 1];
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

    let maxPolar = this._pickDistance(this._maxValue(data, MAX_DISTANCE_IDX));
    const legend = formatUnit(maxPolar, "m");

    this._drawGrid(ctx, cx, cy, rMax, n, legend);
    this._plane(ctx, cx, cy);

    this._drawDatapoints(ctx, data, MAX_DISTANCE_IDX, 2, this._colorSchema.maxDistance, maxPolar);
    this._drawDatapoints(ctx, data, AVG_DISTANCE_IDX, 1, this._colorSchema.avgDistance, maxPolar);
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

    // Render monospace for numeric values or bitStrings
    let style = "";
    if (isNumeric || isBitString) {
      style += "font-family: monospace;";
    }

    // Error tags will be rendered orange when the value is != 0
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
    // Variables starting with a _ are hidden
    if (item.name.startsWith('_')) return html``;

    // Handle special case for polar diagram
    if (item.name.endsWith(":AntPolar") && Array.isArray(item.value)) {
      const id = `polar-${item.name}`;

      const avgDistance = formatUnit(this._maxValue(item.value, AVG_DISTANCE_IDX), "m");
      const maxDistance = formatUnit(this._maxValue(item.value, MAX_DISTANCE_IDX), "m");

      const name = item.name.split(':')[0];

      return html`
      <tr>
        <th style="width:33%" scope="row">${name}</th>
        <td>
          <div style="display:flex; align-items:center; gap:10px">
            <canvas ref="${id}" width="120" height="120"></canvas>
            <div style="font-size:11px; line-height:1.4">
              <div>
                <span style="display:inline-block;width:12px;height:3px;background:${this._colorSchema.avgDistance};margin-right:6px"></span>
                Avg Distance (${avgDistance})
              </div>

              <div>
                <span style="display:inline-block;width:12px;height:3px;background:${this._colorSchema.maxDistance};margin-right:6px"></span>
                Max Distance (${maxDistance})
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
