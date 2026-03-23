import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: overviewCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: overviewCol
            width: parent.width
            spacing: 16

            // Key metrics cards row
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                StatCard { title: "Avg Reward"; value: training.avgStepReward.toFixed(4); accentColor: Theme.accent }
                StatCard { title: "Overall SPS"; value: Math.round(training.overallSPS).toLocaleString(); accentColor: Theme.accentGreen }
                StatCard { title: "Entropy"; value: training.policyEntropy.toFixed(3); accentColor: Theme.accentOrange }
                StatCard { title: "KL Div"; value: training.klDivergence.toFixed(5); accentColor: Theme.accentPurple }
            }

            // Reward history mini-plot
            CardFrame {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                title: "Reward History"

                MiniPlot {
                    anchors.fill: parent
                    anchors.margins: 8
                    anchors.topMargin: 28
                    dataPoints: training.rewardHistory
                    lineColor: Theme.accent
                }
            }

            // SPS history mini-plot
            CardFrame {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                title: "SPS History"

                MiniPlot {
                    anchors.fill: parent
                    anchors.margins: 8
                    anchors.topMargin: 28
                    dataPoints: training.spsHistory
                    lineColor: Theme.accentGreen
                }
            }

            // Reward breakdown
            CardFrame {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(200, rewardGrid.height + 40)
                title: "Reward Breakdown"

                GridLayout {
                    id: rewardGrid
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    anchors.topMargin: 28
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 4

                    Repeater {
                        model: training.rewardBreakdown

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: modelData.name
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                Layout.preferredWidth: 200
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 4
                                radius: 2
                                color: Theme.bgTertiary

                                Rectangle {
                                    property real absVal: Math.abs(modelData.value)
                                    property real maxVal: getMaxReward()
                                    width: parent.width * Math.min(1.0, maxVal > 0 ? absVal / maxVal : 0)
                                    height: parent.height
                                    radius: 2
                                    color: modelData.value >= 0 ? Theme.accentGreen : Theme.accentRed
                                }
                            }

                            Text {
                                text: modelData.value.toFixed(4)
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                font.family: "Consolas"
                                Layout.preferredWidth: 70
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }
            }
        }
    }

    function getMaxReward() {
        var maxVal = 0.001
        var breakdown = training.rewardBreakdown
        for (var i = 0; i < breakdown.length; i++) {
            var v = Math.abs(breakdown[i].value)
            if (v > maxVal) maxVal = v
        }
        return maxVal
    }

    // Stat card component
    component StatCard: Rectangle {
        property string title: ""
        property string value: ""
        property color accentColor: Theme.accent

        Layout.fillWidth: true
        height: 80
        radius: 8
        color: Theme.bgCard
        border.width: 1
        border.color: Theme.border

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4

            Text {
                text: title
                color: Theme.textMuted
                font.pixelSize: 11
                font.weight: Font.Medium
            }

            Item { Layout.fillHeight: true }

            Text {
                text: value
                color: parent.parent.accentColor
                font.pixelSize: 22
                font.weight: Font.Bold
                font.family: "Consolas"
            }
        }
    }

    // Card frame
    component CardFrame: Rectangle {
        property string title: ""
        default property alias content: contentArea.data

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

        Item {
            id: contentArea
            anchors.fill: parent
        }
    }
}
