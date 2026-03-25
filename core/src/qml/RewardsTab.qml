import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: rewardsRoot

    // Freeze model while editing to prevent Repeater rebuild from destroying the edit
    property var _breakdownModel: []
    property bool _editingWeight: false

    Connections {
        target: training
        function onMetricsUpdated() {
            if (!rewardsRoot._editingWeight)
                rewardsRoot._breakdownModel = training.rewardBreakdown
        }
        function onRewardWeightsChanged() {
            if (!rewardsRoot._editingWeight)
                rewardsRoot._breakdownModel = training.rewardBreakdown
        }
    }

    Component.onCompleted: _breakdownModel = training.rewardBreakdown

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: rewardsCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        TapHandler { onTapped: rewardsRoot.forceActiveFocus() }

        ColumnLayout {
            id: rewardsCol
            width: parent.width
            spacing: 8

            Text {
                text: "REWARD BREAKDOWN"
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

            // Header row
            RowLayout {
                Layout.fillWidth: true
                spacing: 0

                Text {
                    text: "Reward"
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    Layout.preferredWidth: 220
                }
                Text {
                    text: "Weight"
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    Layout.preferredWidth: 80
                    horizontalAlignment: Text.AlignRight
                }
                Text {
                    text: "Avg Value"
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    Layout.preferredWidth: 100
                    horizontalAlignment: Text.AlignRight
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "% of Total"
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    Layout.preferredWidth: 250
                    horizontalAlignment: Text.AlignCenter
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.border
            }

            Repeater {
                model: rewardsRoot._breakdownModel

                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: 4
                    color: index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.02)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        spacing: 0

                        Text {
                            text: modelData.name
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            Layout.preferredWidth: 220
                            elide: Text.ElideRight
                        }

                        // Weight input
                        TextField {
                            id: weightField
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 28
                            visible: modelData.weight >= 0
                            text: modelData.weight >= 0 ? modelData.weight.toFixed(4) : ""
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.family: "Consolas"
                            selectByMouse: true
                            horizontalAlignment: Text.AlignRight

                            background: Rectangle {
                                radius: 4
                                color: weightField.activeFocus ? Theme.bgTertiary : "transparent"
                                border.width: 1
                                border.color: weightField.activeFocus ? Theme.accent : Theme.border
                            }

                            onActiveFocusChanged: {
                                if (activeFocus)
                                    rewardsRoot._editingWeight = true
                            }

                            onEditingFinished: {
                                var val = parseFloat(text)
                                if (!isNaN(val))
                                    training.setRewardWeight(modelData.name, val)
                                rewardsRoot._editingWeight = false
                                rewardsRoot._breakdownModel = training.rewardBreakdown
                            }
                        }

                        Text {
                            text: modelData.value.toFixed(5)
                            color: modelData.value >= 0 ? Theme.accentGreen : Theme.accentRed
                            font.pixelSize: 13
                            font.family: "Consolas"
                            Layout.preferredWidth: 100
                            horizontalAlignment: Text.AlignRight
                        }

                        Item { Layout.fillWidth: true }

                        // Percentage bar
                        Item {
                            Layout.preferredWidth: 250
                            Layout.preferredHeight: 20

                            property real weightedVal: modelData.value * Math.abs(modelData.weight)
                            property real totalVal: getTotalAbsWeightedReward()
                            property real pct: totalVal > 0 ? weightedVal / totalVal * 100 : 0

                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: 190
                                height: 14
                                radius: 4
                                color: Theme.bgTertiary

                                Rectangle {
                                    width: Math.abs(parent.parent.pct) / 100 * parent.width
                                    height: parent.height
                                    radius: 4
                                    color: modelData.value >= 0 ? Theme.accentGreen : Theme.accentRed
                                    opacity: 0.85
                                }
                            }

                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: parent.pct.toFixed(1) + "%"
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                font.family: "Consolas"
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }

            // Empty state
            Text {
                visible: rewardsRoot._breakdownModel.length === 0
                text: "Waiting for first iteration..."
                color: Theme.textMuted
                font.pixelSize: 13
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 40
            }
        }
    }

    function getTotalAbsWeightedReward() {
        var total = 0
        var breakdown = rewardsRoot._breakdownModel
        for (var i = 0; i < breakdown.length; i++)
            total += Math.abs(breakdown[i].value * breakdown[i].weight)
        return total
    }
}
