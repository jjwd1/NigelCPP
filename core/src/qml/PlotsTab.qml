import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: plotsCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: plotsCol
            width: parent.width
            spacing: 16

            // Row 1: Reward + SPS
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                PlotCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                    title: "Average Step Reward"
                    dataPoints: training.rewardHistory
                    lineColor: Theme.accent
                }

                PlotCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                    title: "Steps Per Second"
                    dataPoints: training.spsHistory
                    lineColor: Theme.accentGreen
                }
            }

            // Row 2: Entropy + Policy Loss
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                PlotCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                    title: "Policy Entropy"
                    dataPoints: training.entropyHistory
                    lineColor: Theme.accentOrange
                }

                PlotCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                    title: "Policy Loss"
                    dataPoints: training.policyLossHistory
                    lineColor: Theme.accentRed
                }
            }

            // Row 3: Critic Loss
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                PlotCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                    title: "Critic Loss"
                    dataPoints: training.criticLossHistory
                    lineColor: Theme.accentPurple
                }

                // Placeholder for symmetry
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                }
            }
        }
    }

    component PlotCard: Rectangle {
        property string title: ""
        property var dataPoints: []
        property color lineColor: Theme.accent

        radius: 8
        color: Theme.bgCard
        border.width: 1
        border.color: Theme.border

        Text {
            x: 12
            y: 8
            text: title
            color: Theme.textMuted
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 0.5
        }

        MiniPlot {
            anchors.fill: parent
            anchors.margins: 12
            anchors.topMargin: 30
            dataPoints: parent.dataPoints
            lineColor: parent.lineColor
        }
    }
}
