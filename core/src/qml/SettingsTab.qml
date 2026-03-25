import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: settingsRoot

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: settingsCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        TapHandler { onTapped: settingsRoot.forceActiveFocus() }

        ColumnLayout {
            id: settingsCol
            width: parent.width
            spacing: 12

            // ── ENVIRONMENT ──
            SectionHeader { title: "ENVIRONMENT" }

            SettingField {
                label: "Parallel Games"; value: training.cfgNumGames
                tip: "Number of simultaneous game environments. More games = more experience per iteration but more CPU/RAM usage."
                enabled: !training.isTraining
                onEdited: training.cfgNumGames = parseInt(newValue)
            }
            SettingField {
                label: "Tick Skip"; value: training.cfgTickSkip
                tip: "Physics ticks between each agent decision. 8 = decision every ~67ms (120Hz physics). Lower = finer control but slower training."
                enabled: !training.isTraining
                onEdited: training.cfgTickSkip = parseInt(newValue)
            }
            SettingDropdown {
                label: "Device"
                tip: "Hardware to run neural network inference and training on. GPU (CUDA) is vastly faster than CPU."
                options: ["Auto", "CPU", "GPU (CUDA)"]
                selectedIndex: training.cfgDeviceType
                enabled: !training.isTraining
                onIndexChanged: training.cfgDeviceType = newIndex
            }
            SettingField {
                label: "Random Seed"; value: training.cfgRandomSeed
                tip: "Seed for random number generators. Same seed = reproducible training runs."
                enabled: !training.isTraining
                onEdited: training.cfgRandomSeed = parseInt(newValue)
            }

            Item { Layout.preferredHeight: 4 }

            // ── PPO ──
            SectionHeader { title: "PPO" }

            SettingField {
                label: "Steps / Iteration"; value: training.cfgTsPerItr
                tip: "Timesteps of experience collected per training iteration. More steps = more stable gradients but longer iterations."
                enabled: !training.isTraining
                onEdited: training.cfgTsPerItr = parseInt(newValue)
            }
            SettingField {
                label: "Batch Size"; value: training.cfgBatchSize
                tip: "Number of timesteps used per gradient update. Usually equal to Steps/Iteration."
                enabled: !training.isTraining
                onEdited: training.cfgBatchSize = parseInt(newValue)
            }
            SettingField {
                label: "Mini Batch Size"; value: training.cfgMiniBatchSize
                tip: "Batch is split into mini-batches of this size for GPU memory efficiency. Must divide batch size evenly."
                enabled: !training.isTraining
                onEdited: training.cfgMiniBatchSize = parseInt(newValue)
            }
            SettingField {
                label: "Epochs"; value: training.cfgEpochs
                tip: "Number of passes over the collected experience per iteration. More epochs = more learning per iteration but risk of overfitting to stale data."
                enabled: !training.isTraining
                onEdited: training.cfgEpochs = parseInt(newValue)
            }
            SettingField {
                label: "Clip Range"; value: training.cfgClipRange; decimals: 2
                tip: "PPO clipping parameter. Limits how much the policy can change per update. Default 0.2. Lower = more conservative updates."
                enabled: !training.isTraining
                onEdited: training.cfgClipRange = parseFloat(newValue)
            }
            SettingField {
                label: "Policy Temperature"; value: training.cfgPolicyTemp; decimals: 2
                tip: "Scales policy logits before softmax. >1 = more random exploration, <1 = more greedy. Default 1.0."
                enabled: !training.isTraining
                onEdited: training.cfgPolicyTemp = parseFloat(newValue)
            }

            Item { Layout.preferredHeight: 4 }

            // ── LEARNING RATES ──
            SectionHeader { title: "LEARNING RATES" }

            SettingField {
                label: "Policy LR"; value: training.cfgPolicyLR; decimals: 6
                tip: "Learning rate for the policy network optimizer. Controls how fast the policy updates. Too high = instability, too low = slow learning."
                enabled: !training.isTraining
                onEdited: training.cfgPolicyLR = parseFloat(newValue)
            }
            SettingField {
                label: "Critic LR"; value: training.cfgCriticLR; decimals: 6
                tip: "Learning rate for the critic (value) network optimizer. Usually set equal to or slightly higher than policy LR."
                enabled: !training.isTraining
                onEdited: training.cfgCriticLR = parseFloat(newValue)
            }

            Item { Layout.preferredHeight: 4 }

            // ── GAE / ENTROPY ──
            SectionHeader { title: "GAE / ENTROPY" }

            SettingField {
                label: "Entropy Scale"; value: training.cfgEntropyScale; decimals: 4
                tip: "Weight of the entropy bonus in the policy loss. Higher = more exploration. Too high = random behavior, too low = premature convergence."
                enabled: !training.isTraining
                onEdited: training.cfgEntropyScale = parseFloat(newValue)
            }
            SettingField {
                label: "GAE Gamma"; value: training.cfgGaeGamma; decimals: 4
                tip: "Discount factor for future rewards. Higher (closer to 1) = agent cares more about long-term rewards. 0.997 is quite far-sighted."
                enabled: !training.isTraining
                onEdited: training.cfgGaeGamma = parseFloat(newValue)
            }
            SettingField {
                label: "GAE Lambda"; value: training.cfgGaeLambda; decimals: 3
                tip: "GAE smoothing parameter. Controls bias-variance tradeoff in advantage estimation. 1.0 = high variance, 0.0 = high bias. Default 0.95."
                enabled: !training.isTraining
                onEdited: training.cfgGaeLambda = parseFloat(newValue)
            }
            SettingField {
                label: "Reward Clip Range"; value: training.cfgRewardClipRange; decimals: 1
                tip: "Clips per-step rewards to [-range, +range] before computing advantages. Prevents extreme reward spikes from destabilizing training."
                enabled: !training.isTraining
                onEdited: training.cfgRewardClipRange = parseFloat(newValue)
            }

            Item { Layout.preferredHeight: 4 }

            // ── SELF-PLAY ──
            SectionHeader { title: "SELF-PLAY" }

            SettingToggle {
                label: "Train vs Old Versions"
                tip: "When enabled, some games pit the current policy against saved older versions of itself. Prevents forgetting and improves robustness."
                checked: training.cfgTrainAgainstOld
                enabled: !training.isTraining
                onUserChanged: training.cfgTrainAgainstOld = newChecked
            }
            SettingField {
                label: "Old Version Chance"; value: training.cfgTrainAgainstOldChance; decimals: 2
                tip: "Probability that an iteration uses an old opponent instead of self-play. 0.15 = 15% of iterations."
                enabled: !training.isTraining
                visible: training.cfgTrainAgainstOld
                onEdited: training.cfgTrainAgainstOldChance = parseFloat(newValue)
            }
            SettingField {
                label: "Checkpoints to Keep"; value: training.cfgCheckpointsToKeep
                tip: "Maximum number of saved checkpoints on disk. Oldest checkpoints are deleted when this limit is exceeded."
                enabled: !training.isTraining
                onEdited: training.cfgCheckpointsToKeep = parseInt(newValue)
            }
            Item { Layout.preferredHeight: 4 }

            // ── PERFORMANCE ──
            SectionHeader { title: "PERFORMANCE" }

            SettingToggle {
                label: "Half Precision (BF16)"
                tip: "Use BFloat16 for inference during experience collection. Halves memory bandwidth with negligible precision loss. Training gradients stay FP32."
                checked: training.cfgHalfPrecision
                enabled: !training.isTraining
                onUserChanged: training.cfgHalfPrecision = newChecked
            }
            SettingToggle {
                label: "Standardize Observations"
                tip: "Normalizes observations using running mean/std so all input features are on a similar scale. Do NOT enable mid-training."
                checked: training.cfgStandardizeObs
                enabled: !training.isTraining
                onUserChanged: training.cfgStandardizeObs = newChecked
            }
            SettingToggle {
                label: "Standardize Returns"
                tip: "Normalizes GAE returns by their running standard deviation. Keeps critic targets in a stable range as reward magnitudes change."
                checked: training.cfgStandardizeReturns
                enabled: !training.isTraining
                onUserChanged: training.cfgStandardizeReturns = newChecked
            }

            Item { Layout.preferredHeight: 4 }

            // ── NETWORK ARCHITECTURE (read-only) ──
            SectionHeader { title: "NETWORK ARCHITECTURE" }

            Text {
                text: training.networkInfo()
                color: Theme.textSecondary
                font.pixelSize: 13
                font.family: "Consolas"
                lineHeight: 1.6
                Layout.fillWidth: true
                Layout.leftMargin: 4
            }

            InfoRow { label: "Activation"; value: "LeakyReLU" }
            InfoRow { label: "Optimizer"; value: "Adam" }
            InfoRow { label: "Layer Norm"; value: "Enabled" }

            Item { Layout.preferredHeight: 20 }
        }
    }

    // ── Helper ──
    function formatFieldValue(v, decimals) {
        if (typeof v === 'undefined' || v === null) return "0"
        if (decimals === 0) return Math.round(v).toString()
        if (Math.abs(v) > 0 && Math.abs(v) < 0.001) return v.toExponential(2)
        return parseFloat(v.toFixed(decimals)).toString()
    }

    // ── Info icon with tooltip (shared helper) ──
    component InfoIcon: Item {
        property string tip: ""
        visible: tip !== ""
        width: 16
        height: 16

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: infoMouse.containsMouse ? Theme.bgTertiary : "transparent"
            border.width: 1
            border.color: Theme.textMuted

            Text {
                anchors.centerIn: parent
                text: "?"
                color: Theme.textMuted
                font.pixelSize: 10
                font.weight: Font.Bold
            }
        }

        MouseArea {
            id: infoMouse
            anchors.fill: parent
            hoverEnabled: true
        }

        ToolTip {
            id: infoTip
            visible: infoMouse.containsMouse
            delay: 300

            contentItem: Text {
                text: tip
                color: Theme.textPrimary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                width: 260
            }

            background: Rectangle {
                color: Theme.bgCard
                border.width: 1
                border.color: Theme.border
                radius: 6
            }
        }
    }

    // ── Inline Components ──

    component SectionHeader: ColumnLayout {
        property string title: ""
        Layout.fillWidth: true
        spacing: 6

        Text {
            text: title
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
    }

    component SettingField: RowLayout {
        id: fieldRoot
        property string label: ""
        property var value: 0
        property int decimals: 0
        property string tip: ""
        signal edited(string newValue)
        Layout.fillWidth: true
        spacing: 8
        opacity: enabled ? 1.0 : 0.5

        Text {
            text: fieldRoot.label
            color: Theme.textSecondary
            font.pixelSize: 13
            Layout.preferredWidth: 180
        }

        InfoIcon { tip: fieldRoot.tip }

        TextField {
            id: inputField
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            text: settingsRoot.formatFieldValue(fieldRoot.value, fieldRoot.decimals)
            color: Theme.textPrimary
            font.pixelSize: 13
            font.family: "Consolas"
            selectByMouse: true
            enabled: fieldRoot.enabled

            background: Rectangle {
                radius: 4
                color: inputField.enabled ? Theme.bgTertiary : Theme.bgSecondary
                border.width: inputField.activeFocus ? 1 : 0
                border.color: Theme.accent
            }

            onEditingFinished: fieldRoot.edited(text)
        }
    }

    component SettingToggle: RowLayout {
        id: toggleRoot
        property string label: ""
        property bool checked: false
        property string tip: ""
        signal userChanged(bool newChecked)
        Layout.fillWidth: true
        spacing: 8
        opacity: enabled ? 1.0 : 0.5

        Text {
            text: toggleRoot.label
            color: Theme.textSecondary
            font.pixelSize: 13
            Layout.preferredWidth: 180
        }

        InfoIcon { tip: toggleRoot.tip }

        Switch {
            id: switchCtrl
            checked: toggleRoot.checked
            enabled: toggleRoot.enabled
            onToggled: toggleRoot.userChanged(checked)

            indicator: Rectangle {
                implicitWidth: 40
                implicitHeight: 20
                x: switchCtrl.leftPadding
                y: parent.height / 2 - height / 2
                radius: 10
                color: switchCtrl.checked ? Theme.accent : Theme.bgTertiary
                border.width: 1
                border.color: Theme.border

                Rectangle {
                    x: switchCtrl.checked ? parent.width - width - 2 : 2
                    y: 2
                    width: 16
                    height: 16
                    radius: 8
                    color: Theme.textPrimary
                    Behavior on x { NumberAnimation { duration: 100 } }
                }
            }
        }
    }

    component SettingDropdown: RowLayout {
        id: dropdownRoot
        property string label: ""
        property var options: []
        property int selectedIndex: 0
        property string tip: ""
        signal indexChanged(int newIndex)
        Layout.fillWidth: true
        spacing: 8
        opacity: enabled ? 1.0 : 0.5

        Text {
            text: dropdownRoot.label
            color: Theme.textSecondary
            font.pixelSize: 13
            Layout.preferredWidth: 180
        }

        InfoIcon { tip: dropdownRoot.tip }

        ComboBox {
            id: comboField
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            model: dropdownRoot.options
            currentIndex: dropdownRoot.selectedIndex
            enabled: dropdownRoot.enabled
            onActivated: function(index) { dropdownRoot.indexChanged(index) }

            contentItem: Text {
                leftPadding: 8
                text: comboField.displayText
                color: Theme.textPrimary
                font.pixelSize: 13
                font.family: "Consolas"
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 4
                color: comboField.enabled ? Theme.bgTertiary : Theme.bgSecondary
                border.width: comboField.activeFocus ? 1 : 0
                border.color: Theme.accent
            }

            indicator: Canvas {
                x: comboField.width - width - 8
                y: comboField.topPadding + (comboField.availableHeight - height) / 2
                width: 10
                height: 6
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.fillStyle = Theme.textSecondary
                    ctx.beginPath()
                    ctx.moveTo(0, 0)
                    ctx.lineTo(width, 0)
                    ctx.lineTo(width / 2, height)
                    ctx.closePath()
                    ctx.fill()
                }
            }
        }
    }

    component InfoRow: RowLayout {
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        Layout.leftMargin: 4
        spacing: 8

        Text {
            text: label
            color: Theme.textMuted
            font.pixelSize: 12
            Layout.preferredWidth: 120
        }
        Text {
            text: value
            color: Theme.textSecondary
            font.pixelSize: 13
            font.family: "Consolas"
        }
    }
}
