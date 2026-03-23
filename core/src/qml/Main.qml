import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1400
    height: 900
    minimumWidth: 1000
    minimumHeight: 700
    title: "NigelTrain"
    color: Theme.bgPrimary

    // Click anywhere to clear text field focus (commits pending edits)
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: function(mouse) {
            root.contentItem.forceActiveFocus()
            mouse.accepted = false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top bar
        TopBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
        }

        // Main content
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left sidebar - metrics
            MetricsPanel {
                Layout.preferredWidth: 320
                Layout.fillHeight: true
            }

            // Separator
            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: Theme.border
            }

            // Center content - tabs
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                TabBar {
                    id: tabBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    background: Rectangle {
                        color: Theme.bgSecondary
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }
                    }

                    NigelTab { text: "Settings" }
                    NigelTab { text: "Overview" }
                    NigelTab { text: "Plots" }
                    NigelTab { text: "Rewards" }
                    NigelTab { text: "Metrics" }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: tabBar.currentIndex

                    SettingsTab {}
                    OverviewTab {}
                    PlotsTab {}
                    RewardsTab {}
                    MetricsTab {}
                }
            }

            // Separator
            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: Theme.border
            }

            // Right sidebar - GPU
            GPUPanel {
                Layout.preferredWidth: 240
                Layout.fillHeight: true
            }
        }
    }

    // Custom tab button
    component NigelTab: TabButton {
        id: tabBtn
        contentItem: Text {
            text: tabBtn.text
            color: tabBtn.checked ? Theme.accent : Theme.textSecondary
            font.pixelSize: 13
            font.weight: tabBtn.checked ? Font.DemiBold : Font.Normal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: tabBtn.hovered ? Theme.bgTertiary : "transparent"
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 2
                color: tabBtn.checked ? Theme.accent : "transparent"
            }
        }
    }
}
