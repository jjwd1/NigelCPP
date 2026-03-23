import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: Theme.bgSecondary

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Text {
            text: "GPU"
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

        // GPU Available indicator
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: training.gpuAvailable ? Theme.accentGreen : Theme.accentRed
            }

            Text {
                text: training.gpuAvailable ? "Connected" : "Not Available"
                color: training.gpuAvailable ? Theme.accentGreen : Theme.accentRed
                font.pixelSize: 12
            }
        }

        // VRAM bar
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "VRAM"
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: training.gpuVramUsed + " / " + training.gpuVramTotal + " MB"
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    font.family: "Consolas"
                }
            }

            // Progress bar
            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: Theme.bgTertiary

                Rectangle {
                    width: parent.width * (training.gpuVramTotal > 0 ? training.gpuVramUsed / training.gpuVramTotal : 0)
                    height: parent.height
                    radius: 3
                    color: {
                        var pct = training.gpuVramTotal > 0 ? training.gpuVramUsed / training.gpuVramTotal : 0
                        if (pct > 0.9) return Theme.accentRed
                        if (pct > 0.7) return Theme.accentOrange
                        return Theme.accent
                    }

                    Behavior on width { NumberAnimation { duration: 300 } }
                }
            }
        }

        // Utilization bar
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Utilization"
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: training.gpuUtilization + "%"
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    font.family: "Consolas"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: Theme.bgTertiary

                Rectangle {
                    width: parent.width * (training.gpuUtilization / 100)
                    height: parent.height
                    radius: 3
                    color: Theme.accentGreen

                    Behavior on width { NumberAnimation { duration: 300 } }
                }
            }
        }

        // Temperature
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Temperature"
                color: Theme.textSecondary
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Text {
                text: training.gpuTemperature + " C"
                color: {
                    if (training.gpuTemperature > 85) return Theme.accentRed
                    if (training.gpuTemperature > 75) return Theme.accentOrange
                    return Theme.textPrimary
                }
                font.pixelSize: 13
                font.family: "Consolas"
            }
        }

        // Power
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Power Draw"
                color: Theme.textSecondary
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Text {
                text: training.gpuPowerDraw.toFixed(0) + " W"
                color: Theme.textPrimary
                font.pixelSize: 13
                font.family: "Consolas"
            }
        }

        Item { Layout.fillHeight: true }
    }
}
