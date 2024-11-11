import { El } from "@frameable/el";

let instance;

export class OpenAceStore {
  constructor() {
    if (instance) {
      throw new Error("New instance cannot be created!!");
    }

    instance = this;

    this.state = El.observable({
      page: "", // Default startup page
      currentModule: "",
      connected: false,
      aircraftId: "",
      aircraftsObj: {},
      aircrafts: [],
      numberOfAircrafts: 0,
      configModified: false,
    });
  }

  /**
   * Update store aircraft array
   *
   */
  _updateAircraftArray() {
    this.state.aircrafts.length = 0;
    for (var prop in this.state.aircraftsObj) {
      this.state.aircrafts.push({
        name: prop,
        ...this.state.aircraftsObj[prop],
      });
    }
    this.state.numberOfAircrafts = this.state.aircrafts.length;
  }

  /**
   * Initialize the store
   *
   * @returns
   */
  init() {
    return this.fetch("/api/_Configuration/config.json")
      .then((data) => {
        this.state.aircraftId = data.aircraftId;
        this.state.configModified = data._dirty;
        return this.state.aircraftId;
      })
      .then((aircraftId) => {
        this.fetch(`/api/_Configuration/aircraft.json`).then((data) => {
          Object.assign(this.state.aircraftsObj, data);
          this._updateAircraftArray();

          // Validate if the configured aircraft exists in the array
          if (data[this.state.aircraftId] === undefined) {
            if (this.state.aircrafts.length > 0) {
              return this.setDefaultAirCraftId(this.state.aircrafts[0].callSign);
            } else {
              this.state.aircraftId = "";
            }
          }
          return data;
        });
      });
  }

  /**
   * Delete an aircraft by ID from teh aircrafts path
   *
   * @param {*} aircraftId
   * @returns
   */
  deleteAircraft(aircraftId) {
    return this.fetch(`/api/_Configuration/aircraft/${aircraftId}.json`, {
      method: "POST",
      headers: {
        "X-Method": "DELETE",
      },
      body: "{}",
    }).then((data) => {
      delete this.state.aircraftsObj[aircraftId];
      this._updateAircraftArray();
      return this.init();
    });
  }

  /**
   * Update aircraft in the aircrafts path
   *
   * @param {*} aircraft
   * @returns
   */
  updateAircraft(aircraft) {
    return this.fetch(`/api/_Configuration/aircraft/${aircraft.callSign}.json`, {
      method: "POST",
      headers: {
        "X-Method": "POST",
      },
      body: JSON.stringify(aircraft),
    }).then((data) => {
      this.state.aircraftsObj[aircraft.callSign] = {};
      Object.assign(this.state.aircraftsObj[aircraft.callSign], aircraft);
      this._updateAircraftArray();
      return data;
    });
  }

  /**
   * Set the default aircraft ID in the config path
   *
   * @param {aircraftId of the aircraft that needs to be set as default} aircraftId
   * @returns
   */
  setDefaultAirCraftId(aircraftId) {
    const url = "/api/_Configuration/config.json";
    return store
      .fetch(url)
      .then((data) => {
        data.aircraftId = aircraftId;
        return this.fetch(url, {
          method: "POST",
          body: JSON.stringify(data),
        });
      })
      .then((data) => {
        this.state.aircraftId = aircraftId;
        return data;
      });
  }

  getModuleData(moduleName) {
    return store.fetch(`/api/_Configuration/${moduleName}.json`).then((data) => {
      // null means configuration did not exists.
      if (data == null) {
        return {};
      } else {
        return data;
      }
    });
  }

  updateModuleData(moduleName, data) {
    return store.fetch(`/api/_Configuration/${moduleName}.json`, {
      method: "POST",
      body: JSON.stringify(data),
    });
  }
  storeInBRModuleData() {
    return store.fetch(`/api/_Configuration/SaveBr.json`, {
      method: "POST",
      body: {},
    });
  }
  restart() {
    return store.fetch(`/api/_Configuration/Restart.json`, {
      method: "POST",
      body: {},
    });
  }
  usbBoot() {
    return store.fetch(`/api/_Configuration/UsbBoot.json`, {
      method: "POST",
      body: {},
    });
  }

  /**
   * Fetch operation with default handler
   *
   * @param {} path
   * @param {*} requestOptions
   * @returns
   */
  fetch(path, requestOptions) {
    return fetch(path, {
      ...requestOptions,
      signal: AbortSignal.timeout(2500),
    })
      .then((response) => {
        if (path.includes("SaveBR.json")) {
          this.state.configModified = false;
        } else if (["POST", "DELETE", "PATCH"].includes(requestOptions?.method)) {
          this.state.configModified = true;
        }
        if (!response.ok) {
          throw new Error("Network response was not ok");
        }
        this.state.connected = true;
        return response.json();
      })
      .catch((e) => {
        this.state.connected = false;
        throw new Error(e);
      });
  }
}

let store = Object.freeze(new OpenAceStore());
export default store;
