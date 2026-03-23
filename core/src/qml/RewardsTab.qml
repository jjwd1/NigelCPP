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
                    text: "Bar"
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    Layout.preferredWidth: 200
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

                        // Centered bar
                        Item {
                            Layout.preferredWidth: 200
                            Layout.preferredHeight: 12

                            Rectangle {
                                anchors.centerIn: parent
                                width: 200
                                height: 4
                                radius: 2
                                color: Theme.bgTertiary

                                Rectangle {
                                    x: parent.width / 2
                                    width: 1
                                    height: parent.height
                                    color: Theme.borderLight
                                }

                                Rectangle {
                                    property real maxVal: getMaxReward()
                                    property real normalizedVal: maxVal > 0 ? modelData.value / maxVal : 0
                                    property real barWidth: Math.abs(normalizedVal) * parent.width / 2

                                    x: normalizedVal >= 0 ? parent.width / 2 : parent.width / 2 - barWidth
                                    width: barWidth
                                    height: parent.height
                                    radius: 2
                                    color: modelData.value >= 0 ? Theme.accentGreen : Theme.accentRed
                                }
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

    function getMaxReward() {
        var maxVal = 0.001
        var breakdown = rewardsRoot._breakdownModel
        for (var i = 0; i < breakdown.length; i++) {
            var v = Math.abs(breakdown[i].value)
            if (v > maxVal) maxVal = v
        }
        return maxVal
    }
}
