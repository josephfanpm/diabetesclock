// Draws the clock faces on a 32x8 pixel grid the way the firmware does (src/BGDisplayFace*.cpp), so the
// installer page can show what each face looks like before installing.

const MATRIX_WIDTH = 32
const MATRIX_HEIGHT = 8

// RGB565, as in enums.h DISPLAY_COLOR.
const COLOR = {
    BLACK: 0x0000, BLUE: 0x001f, GREEN: 0x07e0, CYAN: 0x07ff, GRAY: 0xa514,
    RED: 0xf800, MAGENTA: 0xf81f, YELLOW: 0xffe0, WHITE: 0xffff,
}

// The firmware's default limits and colors (data/config_initial.json).
const LIMITS = { urgentLow: 55, low: 70, high: 180, urgentHigh: 250 }
const BAND_COLORS = {
    URGENT_LOW: COLOR.RED, WARNING_LOW: COLOR.YELLOW, NORMAL: COLOR.GREEN,
    WARNING_HIGH: COLOR.YELLOW, URGENT_HIGH: COLOR.RED,
}
const DATA_OLD_COLOR = COLOR.GRAY
const DATA_OLD_MINUTES = 20

class Matrix {
    constructor() { this.pixels = new Uint16Array(MATRIX_WIDTH * MATRIX_HEIGHT) }
    clear() { this.pixels.fill(0) }
    pixel(x, y, color) {
        if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) return
        this.pixels[y * MATRIX_WIDTH + x] = color
    }
    get(x, y) { return this.pixels[y * MATRIX_WIDTH + x] }
}

// ---------- text, as DisplayManager::printText and Adafruit GFX draw it ----------
function textWidth(text) {
    let width = 0
    for (const ch of text) {
        const code = ch.charCodeAt(0)
        width += code >= 32 && code < 127 ? FONT.widths[code - 32] : 4
    }
    return width
}

function printText(m, x, y, text, align, color) {
    if (align === "right") x -= textWidth(text)
    else if (align === "center") x = Math.trunc((MATRIX_WIDTH - textWidth(text)) / 2)
    for (const ch of text) {
        const code = ch.charCodeAt(0)
        if (code < 32 || code > 126) continue
        const [offset, w, h, advance, xo, yo] = FONT.glyphs[code - 32]
        let bits = 0, bit = 0, o = offset
        for (let yy = 0; yy < h; yy++) {
            for (let xx = 0; xx < w; xx++) {
                if (!(bit++ & 7)) bits = FONT.bitmaps[o++]
                if (bits & 0x80) m.pixel(x + xo + xx, y + yo + yy, color)
                bits = (bits << 1) & 0xff
            }
        }
        x += advance
    }
}

// Adafruit GFX drawBitmap: rows padded to whole bytes, most significant bit first.
function drawBitmap(m, x, y, rows, w, color) {
    rows.forEach((row, j) => {
        for (let i = 0; i < w; i++) if (row & (0x80 >> i)) m.pixel(x + i, y + j, color)
    })
}

// ---------- glucose ----------
function bgLevel(sgv) {
    if (sgv <= LIMITS.urgentLow) return "URGENT_LOW"
    if (sgv < LIMITS.low) return "WARNING_LOW"
    if (sgv <= LIMITS.high) return "NORMAL"
    if (sgv < LIMITS.urgentHigh) return "WARNING_HIGH"
    return "URGENT_HIGH"
}

// C's round(), which rounds halves away from zero.
const cRound = v => Math.sign(v) * Math.round(Math.abs(v))

function printableReading(sgv, units) {
    return units === "mmol" ? (cRound(sgv / 1.8) / 10).toFixed(1) : String(sgv)
}

const TREND_ARROWS = {
    DOUBLE_UP: [0x50, 0xf8, 0x50, 0x50, 0x50],
    SINGLE_UP: [0x20, 0x70, 0xa8, 0x20, 0x20],
    FORTY_FIVE_UP: [0x38, 0x18, 0x28, 0x40, 0x80],
    FLAT: [0x20, 0x10, 0xf8, 0x10, 0x20],
    FORTY_FIVE_DOWN: [0x80, 0x40, 0x28, 0x18, 0x38],
    SINGLE_DOWN: [0x20, 0x20, 0xa8, 0x70, 0x20],
    DOUBLE_DOWN: [0x50, 0x50, 0x50, 0xf8, 0x50],
}
const ARROW_DATA_OLD = [0x88, 0x50, 0x20, 0x50, 0x88]

function showReading(m, ctx, x, y) {
    const last = ctx.readings[ctx.readings.length - 1]
    const color = ctx.dataIsOld ? DATA_OLD_COLOR : BAND_COLORS[bgLevel(last.sgv)]
    printText(m, x, y, printableReading(last.sgv, ctx.units), "right", color)
}

function showTrendArrow(m, ctx, x, y) {
    const last = ctx.readings[ctx.readings.length - 1]
    if (ctx.dataIsOld) return drawBitmap(m, x, y, ARROW_DATA_OLD, 5, DATA_OLD_COLOR)
    drawBitmap(m, x, y, TREND_ARROWS[last.trend] || [0, 0, 0, 0, 0], 5, COLOR.WHITE)
}

function showTrendVerticalLine(m, ctx, x) {
    const trend = ctx.dataIsOld ? "NONE" : ctx.readings[ctx.readings.length - 1].trend
    const lines = {
        DOUBLE_UP: [[0, COLOR.RED], [1, COLOR.YELLOW], [2, COLOR.GREEN], [3, COLOR.WHITE]],
        DOUBLE_DOWN: [[4, COLOR.WHITE], [5, COLOR.GREEN], [6, COLOR.YELLOW], [7, COLOR.RED]],
        SINGLE_UP: [[1, COLOR.YELLOW], [2, COLOR.GREEN], [3, COLOR.WHITE]],
        SINGLE_DOWN: [[4, COLOR.WHITE], [5, COLOR.GREEN], [6, COLOR.YELLOW]],
        FORTY_FIVE_UP: [[2, COLOR.GREEN], [3, COLOR.WHITE]],
        FORTY_FIVE_DOWN: [[4, COLOR.WHITE], [5, COLOR.GREEN]],
        FLAT: [[3, COLOR.WHITE], [4, COLOR.WHITE]],
    }
    for (const [y, color] of lines[trend] || []) m.pixel(x, y, color)
}

// Up to five blocks, one per minute since the reading.
function drawTimerBlocks(m, ctx, width, x, y) {
    const secondsAgo = ctx.ageSeconds
    const blocks = Math.min(5, Math.floor(secondsAgo / 60))
    if (blocks <= 0) return
    const size = Math.trunc((width - 4) / 5)
    if (size < 1) return
    x += Math.trunc((width - (size * 5 + 4)) / 2)
    const color = secondsAgo >= 60 * DATA_OLD_MINUTES ? DATA_OLD_COLOR : secondsAgo >= 6 * 60 ? COLOR.YELLOW : COLOR.GREEN
    for (let i = 0; i < blocks; i++) for (let j = 0; j < size; j++) m.pixel(x + i * (size + 1) + j, y, color)
}

// Arduino map(), with long integer division.
const arduinoMap = (x, inMin, inMax, outMin, outMax) => Math.trunc((x - inMin) * (outMax - outMin) / (inMax - inMin)) + outMin

function showGraph(m, ctx, x, length, minutes) {
    const pixelSeconds = Math.trunc(minutes * 60 / length)
    for (let i = 0; i < length; i++) {
        const from = ctx.now - i * pixelSeconds, to = ctx.now - (i + 1) * pixelSeconds
        const inPixel = ctx.readings.filter(r => r.epoch >= to && r.epoch < from)
        if (!inPixel.length) continue
        const average = Math.trunc(inPixel.reduce((sum, r) => sum + r.sgv, 0) / inPixel.length)
        const level = bgLevel(average)
        const y = { URGENT_HIGH: 0, WARNING_HIGH: 1, WARNING_LOW: 6, URGENT_LOW: 7 }[level]
            ?? 5 - arduinoMap(average, LIMITS.low, LIMITS.high, 0, 4)
        m.pixel(x + length - 1 - i, y, BAND_COLORS[level])
    }
}

// ---------- faces ----------
function faceGraphAndValue(m, ctx) {
    const value = printableReading(ctx.readings[ctx.readings.length - 1].sgv, ctx.units)
    const valueWidth = textWidth(value)
    const graphWidth = MATRIX_WIDTH - valueWidth - 2
    showGraph(m, ctx, 0, graphWidth, graphWidth * 5)
    showReading(m, ctx, 31, 6)
    showTrendVerticalLine(m, ctx, 31)
    drawTimerBlocks(m, ctx, valueWidth + 2, graphWidth, 7)
}

function getDiff(ctx) {
    const r = ctx.readings
    if (r.length < 2) return ""
    const last = r[r.length - 1]
    let found = []
    for (let i = r.length - 1; i >= 0 && last.epoch - r[i].epoch <= 6 * 60 + 30; i--) found.push(r[i])
    if (found.length > 5) found = found.slice(0, 2)
    const min = Math.min(...found.map(x => x.sgv)), max = Math.max(...found.map(x => x.sgv))
    if (min !== last.sgv && max !== last.sgv) return "?"
    const diff = min === last.sgv ? last.sgv - max : last.sgv - min
    if (Math.abs(diff) > 99) return "?"
    return (diff >= 0 ? "+" : "") + printableReading(diff, ctx.units)
}

function faceValueAndDelta(m, ctx) {
    const last = ctx.readings[ctx.readings.length - 1]
    showReading(m, ctx, ctx.units === "mmol" && last.sgv >= 180 ? 14 : 13, 6)
    showTrendArrow(m, ctx, 13, 1)
    printText(m, 33, 6, getDiff(ctx), "right", ctx.dataIsOld ? DATA_OLD_COLOR : COLOR.WHITE)
    drawTimerBlocks(m, ctx, MATRIX_WIDTH, 0, 7)
}

function faceClockAndValue(m, ctx) {
    let hour = ctx.time.getHours()
    if (ctx.timeFormat === "12") {
        const pm = hour >= 12
        hour = hour % 12 === 0 ? 12 : hour % 12
        for (let i = 0; i < 16; i++) m.pixel(i, 7, pm ? COLOR.BLUE : COLOR.CYAN)
    }
    printText(m, 0, 6, String(hour).padStart(2, "0"), "left", COLOR.WHITE)
    printText(m, 9, 6, String(ctx.time.getMinutes()).padStart(2, "0"), "left", COLOR.WHITE)
    showReading(m, ctx, 31, 6)
    if (ctx.timeFormat === "12") drawTimerBlocks(m, ctx, MATRIX_WIDTH - 17, 18, 7)
    else drawTimerBlocks(m, ctx, MATRIX_WIDTH, 0, 7)
    showTrendVerticalLine(m, ctx, 31)
}

function faceTimeOnly(m, ctx) {
    const t = ctx.time, pad = n => String(n).padStart(2, "0")
    const text = ctx.timeFormat === "12"
        ? `${t.getHours() % 12 === 0 ? 12 : t.getHours() % 12}:${pad(t.getMinutes())} ${t.getHours() < 12 ? "AM" : "PM"}`
        : `${pad(t.getHours())}:${pad(t.getMinutes())}:${pad(t.getSeconds())}`
    printText(m, 0, 6, text, "center", COLOR.WHITE)
}

// ---------- weather (src/BGDisplayFaceWeather.cpp) ----------
const WEATHER_PALETTE = [0xffe0, 0xfd20, 0xffff, 0x8410, 0x04bf, 0xc618]
const WEATHER_ICONS = {
    sun: "20020020 02020200 00111000 22111220 00111000 02020200 20020020 00000000",
    moon: "00111000 01100000 11000003 11000000 11000000 01100010 00111100 00000000",
    partly: "20200000 01100000 21133000 00333300 03333330 33333333 04444440 00000000",
    cloud: "00000000 00033000 00333300 03333330 33333333 33333333 04444440 00000000",
    fog: "00000000 06666660 00000000 66666600 00000000 00666666 00000000 06666600",
    rain: "00033000 00333300 03333330 33333333 04444440 05005050 50050500 00000000",
    snow: "00066000 00666600 06666660 66666666 04444440 03003003 00000000 30030030",
    storm: "00044000 00444400 04444440 44444444 04441440 00011000 00001000 00010000",
}

// WMO weather interpretation codes, as Open-Meteo reports them.
function weatherIcon(code, isDay) {
    if (code === 0) return isDay ? "sun" : "moon"
    if (code <= 2) return "partly"
    if (code === 45 || code === 48) return "fog"
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return "rain"
    if ((code >= 71 && code <= 77) || code === 85 || code === 86) return "snow"
    if (code >= 95) return "storm"
    return "cloud"
}

// The city's initials when it has three or more words, otherwise its first three letters.
function cityCode(city) {
    const name = String(city || "").split(",")[0].trim()
    const initials = name.split(/[\s-]+/).filter(Boolean).map(w => w[0]).join("")
    return (initials.length >= 3 ? initials : name.replace(/[^A-Za-z]/g, "")).slice(0, 3).toUpperCase()
}

function faceWeather(m, ctx) {
    const w = ctx.weather
    const code = cityCode(w.city)
    printText(m, 0, 6, code, "left", COLOR.WHITE)
    const iconX = Math.max(12, textWidth(code))
    if (!w.valid) return printText(m, 33, 6, "--", "right", DATA_OLD_COLOR)
    WEATHER_ICONS[w.icon].split(" ").forEach((row, y) => [...row].forEach((p, x) => {
        if (p !== "0") m.pixel(iconX + x, y, WEATHER_PALETTE[p - 1])
    }))
    const temperature = String(w.temperature)
    if (temperature.length >= 3) return printText(m, 33, 6, temperature, "right", COLOR.WHITE)
    printText(m, 30, 6, temperature, "right", COLOR.WHITE)
    for (const [x, y] of [[30, 1], [31, 1], [30, 2], [31, 2]]) m.pixel(x, y, COLOR.WHITE)
}

const FACES = [
    { id: "graph_and_bg", name: "Glucose graph and value", draw: faceGraphAndValue },
    { id: "value_and_diff", name: "Value and delta", draw: faceValueAndDelta },
    { id: "clock", name: "Current time and BG value", draw: faceClockAndValue },
    { id: "time_only", name: "Time only", draw: faceTimeOnly },
    { id: "weather", name: "Weather", draw: faceWeather },
]

// ---------- sample readings ----------
// Three hours of readings every five minutes, the newest `ageMinutes` old, shaped by the scenario.
function sampleReadings(scenario, ageMinutes, now) {
    const shapes = {
        inRange: t => 118 + 22 * Math.sin(t / 9) + 6 * Math.sin(t / 2.3),
        high: t => 135 + 5 * Math.sin(t / 3) + 6 * Math.max(0, t - 20),
        low: t => 150 + 4 * Math.sin(t / 3) - 6 * Math.max(0, t - 20),
    }
    const shape = shapes[scenario]
    const readings = []
    for (let i = 35; i >= 0; i--) {
        const t = 35 - i
        readings.push({ sgv: Math.round(shape(t)), epoch: now - ageMinutes * 60 - i * 5 * 60 })
    }
    for (let i = 0; i < readings.length; i++) {
        const rate = i ? (readings[i].sgv - readings[i - 1].sgv) / 5 : 0
        readings[i].trend = rate > 3 ? "DOUBLE_UP" : rate > 2 ? "SINGLE_UP" : rate > 1 ? "FORTY_FIVE_UP"
            : rate >= -1 ? "FLAT" : rate >= -2 ? "FORTY_FIVE_DOWN" : rate >= -3 ? "SINGLE_DOWN" : "DOUBLE_DOWN"
    }
    return readings
}
