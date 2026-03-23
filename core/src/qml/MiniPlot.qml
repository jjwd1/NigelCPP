import QtQuick

// Lightweight canvas-based line plot — no QtCharts dependency for these inline plots
Canvas {
    id: plotCanvas

    property var dataPoints: []
    property color lineColor: "#58a6ff"
    property color fillColor: Qt.rgba(lineColor.r, lineColor.g, lineColor.b, 0.08)
    property real lineWidth: 1.5

    onDataPointsChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        if (!dataPoints || dataPoints.length < 2) return

        var pts = dataPoints
        var len = pts.length

        // Find min/max with 5% padding
        var minVal = Number.MAX_VALUE
        var maxVal = -Number.MAX_VALUE
        for (var i = 0; i < len; i++) {
            var v = pts[i]
            if (v < minVal) minVal = v
            if (v > maxVal) maxVal = v
        }

        var range = maxVal - minVal
        if (range < 1e-8) range = 1.0
        var padding = range * 0.05
        minVal -= padding
        maxVal += padding
        range = maxVal - minVal

        var stepX = width / (len - 1)
        var h = height

        // Draw fill
        ctx.beginPath()
        ctx.moveTo(0, h)
        for (var i = 0; i < len; i++) {
            var x = i * stepX
            var y = h - ((pts[i] - minVal) / range) * h
            ctx.lineTo(x, y)
        }
        ctx.lineTo((len - 1) * stepX, h)
        ctx.closePath()
        ctx.fillStyle = fillColor
        ctx.fill()

        // Draw line
        ctx.beginPath()
        for (var i = 0; i < len; i++) {
            var x = i * stepX
            var y = h - ((pts[i] - minVal) / range) * h
            if (i === 0) ctx.moveTo(x, y)
            else ctx.lineTo(x, y)
        }
        ctx.strokeStyle = lineColor
        ctx.lineWidth = lineWidth
        ctx.stroke()

        // Draw latest value label
        var lastVal = pts[len - 1]
        var lastY = h - ((lastVal - minVal) / range) * h
        ctx.fillStyle = lineColor
        ctx.font = "11px Consolas"
        ctx.textAlign = "right"
        ctx.fillText(lastVal.toFixed(4), width - 4, Math.max(14, lastY - 6))

        // Draw min/max guides
        ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.06)
        ctx.lineWidth = 0.5
        ctx.setLineDash([4, 4])

        ctx.beginPath()
        ctx.moveTo(0, 2)
        ctx.lineTo(width, 2)
        ctx.stroke()

        ctx.beginPath()
        ctx.moveTo(0, h - 2)
        ctx.lineTo(width, h - 2)
        ctx.stroke()

        ctx.setLineDash([])
    }
}
