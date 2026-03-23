import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: metricsCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: metricsCol
            width: parent.width
            spacing: 16

            // Skill metrics from step callback
            Text {
                text: "SKILL METRICS"
                color: Theme.textMuted
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 1
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.border
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 12
                rowSpacing: 12

                Repeater {
                    model: training.skillMetrics

                    Rectangle {
                        Layout.fillWidth: true
                        height: 80
                        radius: 8
                        color: Theme.bgCard
                        border.width: 1
                        border.color: Theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            Text {
                                text: modelData.name
                                color: Theme.textSecondary
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Item { Layout.fillHeight: true }

                            Text {
                                text: formatMetricValue(modelData.value)
                                color: Theme.textPrimary
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                                font.family: "Consolas"
                            }

                            // Mini progress bar for ratio metrics (0-1)
                            Rectangle {
                                visible: modelData.name.indexOf("Ratio") >= 0
                                Layout.fillWidth: true
                                height: 3
                                radius: 1.5
                                color: Theme.bgTertiary

                                Rectangle {
                                    width: parent.width * Math.min(1.0, Math.max(0, modelData.value))
                                    height: parent.height
                                    radius: 1.5
                                    color: Theme.accent

                                    Behavior on width { NumberAnimation { duration: 300 } }
                                }
                            }
                        }
                    }
                }
            }

            // Empty state for skill metrics
            Text {
                visible: !training.skillMetrics || training.skillMetrics.length === 0
                text: "Waiting for first iteration..."
                color: Theme.textMuted
                font.pixelSize: 13
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 40
            }

            Item { Layout.preferredHeight: 20 }

            // Console log
            Text {
                text: "CONSOLE"
                color: Theme.textMuted
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 1
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.border
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                radius: 8
                color: Theme.bgCard
                border.width: 1
                border.color: Theme.border

                ListView {
                    id: consoleView
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: training.logMessages

                    delegate: Text {
                        width: consoleView.width
                        text: modelData
                        color: {
                            if (modelData.indexOf("(verified from trainer)") >= 0) return Theme.accentGreen
                            if (modelData.indexOf("(live from trainer)") >= 0) return Theme.accentGreen
                            if (modelData.indexOf("ERROR") >= 0) return Theme.accentRed
                            if (modelData.indexOf("WARNING") >= 0) return Theme.accentOrange
                            return Theme.textSecondary
                        }
                        font.pixelSize: 12
                        font.family: "Consolas"
                        wrapMode: Text.WordWrap
                    }

                    // Auto-scroll to bottom
                    onCountChanged: {
                        Qt.callLater(function() {
                            consoleView.positionViewAtEnd()
                        })
                    }
                }

                // Empty state
                Text {
                    visible: training.logMessages.length === 0
                    anchors.centerIn: parent
                    text: "No log messages yet"
                    color: Theme.textMuted
                    font.pixelSize: 13
                }
            }
        }
    }

    function formatMetricValue(val) {
        if (Math.abs(val) < 0.01) return val.toFixed(4)
        if (Math.abs(val) < 1) return val.toFixed(3)
        if (Math.abs(val) < 100) return val.toFixed(2)
        return Math.round(val).toLocaleString()
    }
}
