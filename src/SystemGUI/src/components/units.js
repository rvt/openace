const UNIT_TABLE = {
    hz: [
        { scale: 1e9, suffix: "GHz", min: 1e9, digits: 1 },
        { scale: 1e6, suffix: "MHz", min: 1e6, digits: 1 },
        { scale: 1e3, suffix: "kHz", min: 1e3, digits: 1 },
        { scale: 1, suffix: "Hz", min: 0, digits: 0 },
    ],

    kb: [
        { scale: 1e6, suffix: "MB", min: 1e6, digits: 1 },
        { scale: 1e3, suffix: "KB", min: 1e3, digits: 1 },
        { scale: 1, suffix: "B", min: 0, digits: 1 },
    ],
    k: [
        { scale: 1e3, suffix: "K", min: 1e3, digits: 2 },
        { scale: 1, suffix: "", min: 0, digits: 0 },
    ],
    deg: [
        { scale: 1, suffix: "Deg", min: 0, digits: 1 },
    ],
    m: [
        { scale: 1e3, suffix: "Km", min: 1e3, digits: 1 },
        { scale: 1, suffix: "m", min: 0, digits: 0 },
    ],
    msec: [
        { scale: 1, suffix: "Ms", min: 0, digits: 0 },
    ],
    ms: [
        { scale: 0.277777778, suffix: "Km/h", min: 2.77777778, digits: 1 },
        { scale: 1, suffix: "m/s", min: 0, digits: 1 },
    ],
    dbm: [
        { scale: 1, suffix: "dBm", min: 0, digits: 0 },
    ],
    psec: [
        { scale: 1, suffix: "per/sec", min: 0, digits: 0 },
    ],
    err: [
        { scale: 1, suffix: " (errors)", min: 0, digits: 0 },
    ],
    // m/s to kt
    kt: [
        { scale: 0.514444444, suffix: "Kt", min: 0, digits: 1 },
    ],
    // meter to miles
    mi: [
        { scale: 1852.02333549, suffix: "Nm", min: 500, digits: 2 },
        { scale: 0.3047999995, suffix: "ft", min: 0, digits: 0 },
    ],
    // meter to ft
    ft: [
        { scale: 0.3047999995, suffix: "ft", min: 0, digits: 0 },
    ],

};

const formatTime = (seconds, locale = navigator.language) => {
    if (typeof seconds !== 'number' || isNaN(seconds) || seconds < 0) {
        return String(seconds);
    }

    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    const parts = [];

    if (hours > 0) {
        parts.push(`${hours}hour${hours !== 1 ? 's' : ''}`);
    }

    if (minutes > 0) {
        parts.push(`${minutes}min${minutes !== 1 ? '' : ''}`);
    }

    if (secs > 0 || parts.length === 0) {
        parts.push(`${secs}sec${secs !== 1 ? 's' : ''}`);
    }

    return parts.join(' ');
};

const findUnit = (name) => {
    const splited = name.split(":");
    if (splited.length == 2) {
        return splited[1];
    }
    return "";
};

const formatUnit = (value, unit, locale = navigator.language) => {

    if (unit === 'b') {
        return value > 0 ? '&#10003;' : '&#x292B;';
    }

    if (unit === 'el') {
        return formatTime(value);
    }

    const table = UNIT_TABLE[unit];
    if (!table) {
        return value;
    }

    for (const entry of table) {
        if (value >= entry.min) {
            const scaled = value / entry.scale;

            const formatter = new Intl.NumberFormat(locale, {
                minimumFractionDigits: entry.digits,
                maximumFractionDigits: entry.digits,
            });

            return formatter.format(scaled) + entry.suffix;
        }
    }
    return value + unit;
};

const formatUnit2 = (name, value, locale = navigator.language) => {
    const u = findUnit(name);
    const v= formatUnit(value, u, locale);
    name = name.replace(`:${u}`,'');
    return {name:name, value:v}
}

export { formatUnit, formatUnit2, findUnit };
