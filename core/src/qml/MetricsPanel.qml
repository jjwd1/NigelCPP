import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: Theme.bgSecondary

    Flickable {
        anchors.fill: parent
        anchors.margins: 12
        contentHeight: metricsCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        TapHandler { onTapped: parent.forceActiveFocus() }

        ColumnLayout {
            id: metricsCol
            width: parent.width
            spacing: 16

            // Performance section
            MetricSection {
                title: "Performance"
                Layout.fillWidth: true

                MetricRow { label: "Overall SPS"; value: Math.round(training.overallSPS).toLocaleString(); unit: "steps/s" }
                MetricRow { label: "Avg SPS (session)"; value: Math.round(training.avgSPS).toLocaleString(); unit: "steps/s"; highlight: true }
                MetricRow { label: "Collection SPS"; value: Math.round(training.collectionSPS).toLocaleString(); unit: "steps/s" }
                MetricRow { label: "Consumption SPS"; value: Math.round(training.consumptionSPS).toLocaleString(); unit: "steps/s" }
            }

            // Training section
            MetricSection {
                title: "Training"
                Layout.fillWidth: true

                MetricRow { label: "Avg Reward"; value: training.avgStepReward.toFixed(4); highlight: true }
                MetricRow { label: "Policy Entropy"; value: training.policyEntropy.toFixed(4) }
                MetricRow { label: "Policy Loss"; value: training.policyLoss.toFixed(6) }
                MetricRow { label: "Critic Loss"; value: training.criticLoss.toFixed(4) }
                MetricRow { label: "KL Divergence"; value: training.klDivergence.toFixed(6) }
                MetricRow { label: "Clip Fraction"; value: training.clipFraction.toFixed(4) }
            }

            // Update magnitudes
            MetricSection {
                title: "Update Magnitudes"
                Layout.fillWidth: true

                MetricRow { label: "Policy"; value: training.policyUpdateMag.toFixed(6) }
                MetricRow { label: "Critic"; value: training.criticUpdateMag.toFixed(6) }
            }

            // Timing
            MetricSection {
                title: "Timing"
                Layout.fillWidth: true

                MetricRow { label: "Collection"; value: training.collectionTime.toFixed(1); unit: "s" }
                MetricRow { label: "Consumption"; value: training.consumptionTime.toFixed(1); unit: "s" }
                MetricRow { label: "GAE"; value: training.gaeTime.toFixed(2); unit: "s" }
                MetricRow { label: "PPO Learn"; value: training.ppoLearnTime.toFixed(2); unit: "s" }
            }
        }
    }

    // Section header
    component MetricSection: ColumnLayout {
        property string title: ""
        spacing: 6

        Text {
            text: title
            color: Theme.textMuted
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 1
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }
    }

    // Metric row
    component MetricRow: RowLayout {
        property string label: ""
        property string value: ""
        property string unit: ""
        property bool highlight: false

        Layout.fillWidth: true
        spacing: 4

        Text {
            text: label
            color: Theme.textSecondary
            font.pixelSize: 12
            Layout.fillWidth: true
        }

        Text {
            text: value
            color: highlight ? Theme.accent : Theme.textPrimary
            font.pixelSize: 13
            font.weight: highlight ? Font.DemiBold : Font.Normal
            font.family: "Consolas"
        }

        Text {
            visible: unit !== ""
            text: unit
            color: Theme.textMuted
            font.pixelSize: 10
        }
    }
}
