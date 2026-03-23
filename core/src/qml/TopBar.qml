import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: Theme.bgSecondary

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 16

        // Logo / Title
        RowLayout {
            spacing: 10

            Rectangle {
                width: 32
                height: 32
                radius: 6
                color: Theme.accent

                Text {
                    anchors.centerIn: parent
                    text: "N"
                    color: "#ffffff"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                }
            }

            Text {
                text: "NigelTrain"
                color: Theme.textPrimary
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
        }

        Item { Layout.fillWidth: true }

        // Status indicator (always visible)
        RowLayout {
            spacing: 8

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: {
                    if (training.status === "RUNNING") return Theme.accentGreen
                    if (training.status === "PAUSED") return Theme.accentOrange
                    if (training.status === "STOPPED") return Theme.accentRed
                    return Theme.textMuted
                }
            }

            Text {
                text: training.status
                color: Theme.textSecondary
                font.pixelSize: 13
                font.weight: Font.Medium
            }
        }

        // Separator (show when there's data or training)
        Rectangle {
            width: 1; height: 28; color: Theme.border
            visible: training.isTraining || training.totalTimesteps > 0
        }

        // Timesteps
        RowLayout {
            visible: training.isTraining || training.totalTimesteps > 0
            spacing: 6
            Text {
                text: "Steps"
                color: Theme.textMuted
                font.pixelSize: 12
            }
            Text {
                text: formatNumber(training.totalTimesteps)
                color: Theme.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.family: "Consolas"
            }
        }

        // Iterations
        RowLayout {
            visible: training.isTraining || training.totalTimesteps > 0
            spacing: 6
            Text {
                text: "Itr"
                color: Theme.textMuted
                font.pixelSize: 12
            }
            Text {
                text: training.totalIterations.toLocaleString()
                color: Theme.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.family: "Consolas"
            }
        }

        // Separator (only when training)
        Rectangle {
            width: 1; height: 28; color: Theme.border
            visible: training.isTraining
        }

        // Visualize button (only when idle)
        NigelButton {
            visible: !training.isTraining
            text: "Visualize"
            onClicked: { forceActiveFocus(); training.visualize() }
        }

        // Start button (only when idle)
        NigelButton {
            visible: !training.isTraining
            text: "Start"
            accent: true
            onClicked: { forceActiveFocus(); training.start() }
        }

        // Controls (only when training)
        RowLayout {
            visible: training.isTraining
            spacing: 8

            NigelButton {
                text: training.status === "PAUSED" ? "Resume" : "Pause"
                accent: training.status === "PAUSED"
                onClicked: training.status === "PAUSED" ? training.resume() : training.pause()
            }

            NigelButton {
                text: "Save"
                onClicked: training.save()
            }

            NigelButton {
                text: "Stop"
                danger: true
                onClicked: training.stop()
            }
        }
    }

    // Helper function
    function formatNumber(n) {
        if (n >= 1e9) return (n / 1e9).toFixed(2) + "B"
        if (n >= 1e6) return (n / 1e6).toFixed(2) + "M"
        if (n >= 1e3) return (n / 1e3).toFixed(1) + "K"
        return n.toString()
    }

    // Button component
    component NigelButton: Button {
        id: btn
        property bool accent: false
        property bool danger: false

        contentItem: Text {
            text: btn.text
            color: btn.danger ? Theme.accentRed : (btn.accent ? Theme.accent : Theme.textPrimary)
            font.pixelSize: 12
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
        }

        background: Rectangle {
            implicitWidth: 64
            implicitHeight: 28
            radius: 6
            color: btn.pressed ? Theme.bgPrimary : (btn.hovered ? Theme.bgTertiary : Theme.bgCard)
            border.width: 1
            border.color: btn.danger ? Qt.darker(Theme.accentRed, 1.5) : Theme.border
        }
    }
}
