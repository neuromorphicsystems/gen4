R"(
    import Chameleon 1.0
    import QtQuick 2.15
    import QtQuick.Layouts 1.15
    import QtQuick.Window 2.15
    import QtQuick.Controls 2.15
    import QtQuick.Controls.Styles 1.4
    import QtTest 1.2
    Window {
        id: window
        visible: true
        width: header_width
        height: header_height
        Timer {
            interval: 16
            running: true
            repeat: true
            onTriggered: {
                if (parameters.use_count_display) {
                    count_display.trigger_draw();
                } else {
                    dvs_display.trigger_draw();
                }
            }
        }
        property var showMenu: false
        property var requestIndex: 0
        RowLayout{
            spacing: 0
            width: window.width
            height: window.height

            Rectangle {
                id: eventsView
                Layout.alignment: Qt.AlignCenter
                Layout.fillHeight: true
                Layout.fillWidth: true
                color: "transparent"

                BackgroundCleaner {
                    width: eventsView.width
                    height: eventsView.height
                    color: '#191919'
                }
                DvsDisplay {
                    objectName: "dvs_display"
                    id: dvs_display
                    width: parameters.use_count_display ? 0 : eventsView.width
                    height: parameters.use_count_display ? 0 : eventsView.height
                    canvas_size: Qt.size(header_width, header_height)
                    parameter: 100000
                    style: DvsDisplay.Linear
                    on_colormap: ['#F4C20D', '#191919']
                    off_colormap: ['#1E88E5', '#191919']
                }
                CountDisplay {
                    objectName: "count_display"
                    id: count_display
                    width: parameters.use_count_display ? eventsView.width : 0
                    height: parameters.use_count_display ? eventsView.height : 0
                    canvas_size: Qt.size(header_width, header_height)
                    colormap: CountDisplay.Hot
                    discard_ratio: 0.001
                }
            }

            Rectangle {
                id: menu
                Layout.alignment: Qt.AlignCenter
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 350
                Layout.maximumWidth: 350
                visible: showMenu
                color: "#292929"

                ScrollView {
                    y: 60
                    width: menu.width
                    height: menu.height - 60
                    clip: true
                    leftPadding: 20
                    rightPadding: 20
                    bottomPadding: 20
                    ColumnLayout {
                        width: menu.width - 40
                        spacing: 0

                        // recordings directory
                        Text {
                            visible: parameters.recording_name == null
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: "Recordings directory"
                            color: "#AAAAAA"
                            font: title_font;
                        }
                        Text {
                            Layout.maximumWidth: 310
                            wrapMode: Text.WordWrap
                            visible: parameters.recording_name == null
                            Layout.bottomMargin: 20
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: parameters.recordings_directory
                            color: "#FFFFFF"
                            font: monospace_font;
                        }

                        // recording
                        Text {
                            visible: parameters.recording_name != null
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: "Recording"
                            color: "#AAAAAA"
                            font: title_font;
                        }
                        Text {
                            Layout.maximumWidth: 310
                            wrapMode: Text.WordWrap
                            visible: parameters.recording_name != null
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: parameters.recording_name ? parameters.recording_name : ""
                            color: "#FFFFFF"
                            font: monospace_font;
                        }
                        Text {
                            visible: parameters.recording_name != null
                            Layout.bottomMargin: 20
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: parameters.recording_status ? parameters.recording_status : ""
                            color: "#FFFFFF"
                            font: monospace_font;
                        }

                        // event rate
                        Text {
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: "Event rate"
                            color: "#AAAAAA"
                            font: title_font;
                        }
                        Text {
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: parameters.event_rate
                            color: "#FFFFFF"
                            font: monospace_font;
                        }
                        Text {
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: parameters.event_rate_on
                            color: "#FFFFFF"
                            font: monospace_font;
                        }
                        Text {
                            Layout.bottomMargin: 20
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: parameters.event_rate_off
                            color: "#FFFFFF"
                            font: monospace_font;
                        }

                        // serial
                        Text {
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: "Serial"
                            color: "#AAAAAA"
                            font: title_font;
                        }
                        Text {
                            Layout.bottomMargin: 20
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: parameters.serial
                            color: "#FFFFFF"
                            font: monospace_font;
                        }

                        // display
                        Text {
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: "Display"
                            color: "#AAAAAA"
                            font: title_font;
                        }
                        Repeater {
                            model: ["Flip bottom-top", "Flip left-right"]
                            RowLayout {
                                Layout.topMargin: 5
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                                spacing: 10
                                Switch {
                                    palette.midlight: "#191919"
                                    palette.window: "#1E88E5"
                                    palette.dark: "#125FA2"
                                    palette.light: "#1E88E5"
                                    palette.mid: "#1E88E5"
                                    onToggled: {
                                        parameters[modelData] = position > 0.5;
                                    }
                                }
                                Text {
                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                                    text: modelData
                                    color: "#CCCCCC"
                                    font: title_font;
                                }
                            }
                        }
                        RowLayout {
                            Layout.topMargin: 5
                            Layout.bottomMargin: 10
                            ComboBox {
                                id: style_combo_box
                                model: ["Exponential", "Linear", "Window", "Count"]
                                currentIndex: 1
                                onCurrentIndexChanged: {
                                    if (currentIndex < 3) {
                                        parameters.use_count_display = false
                                        dvs_display.style = [DvsDisplay.Exponential, DvsDisplay.Linear, DvsDisplay.Window][currentIndex]
                                    } else {
                                        parameters.use_count_display = true
                                    }
                                }
                                palette.button: "#393939"
                                palette.dark: "#CCCCCC"
                                palette.buttonText: "#FFFFFF"
                                palette.highlightedText: "#FFFFFF"
                                palette.mid: "#494949"
                                palette.window: "#191919"
                                palette.text: "#CCCCCC"
                                delegate: ItemDelegate {
                                    id: style_combo_box_delegate
                                    width: parent.width
                                    text: modelData
                                    font: style_combo_box.currentIndex === index ? title_font : base_font
                                    highlighted: style_combo_box.highlightedIndex === index
                                    hoverEnabled: style_combo_box.hoverEnabled
                                    background: Rectangle {
                                        anchors.fill: style_combo_box_delegate
                                        color: style_combo_box_delegate.highlighted ? "#393939" : "transparent"
                                    }
                                    palette.text: "#CCCCCC"
                                    palette.highlightedText: "#FFFFFF"
                                }
                            }
                            Text {
                                Layout.leftMargin: 10
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                                text: "Decay"
                                color: "#CCCCCC"
                                font: title_font
                            }
                        }
                        RowLayout {
                            Layout.topMargin: 5
                            Layout.bottomMargin: 20
                            ComboBox {
                                id: tau_combo_box
                                model: ["50 ms", "100 ms", "200 ms", "500 ms", "1 s", "5 s", "10 s", "50 s", "100 s"]
                                currentIndex: 1
                                onCurrentIndexChanged: {
                                    var tau = [50, 100, 200, 500, 1000, 5000, 10000, 50000, 100000][currentIndex] * 1000;
                                    dvs_display.parameter = tau;
                                    parameters.tau = tau;
                                }
                                palette.button: "#393939"
                                palette.dark: "#CCCCCC"
                                palette.buttonText: "#FFFFFF"
                                palette.highlightedText: "#FFFFFF"
                                palette.mid: "#494949"
                                palette.window: "#191919"
                                palette.text: "#CCCCCC"
                                delegate: ItemDelegate {
                                    id: tau_combo_box_delegate
                                    width: parent.width
                                    text: modelData
                                    font: style_combo_box.currentIndex === index ? title_font : base_font
                                    highlighted: tau_combo_box.highlightedIndex === index
                                    hoverEnabled: tau_combo_box.hoverEnabled
                                    background: Rectangle {
                                        anchors.fill: tau_combo_box_delegate
                                        color: tau_combo_box_delegate.highlighted ? "#393939" : "transparent"
                                    }
                                    palette.text: "#CCCCCC"
                                    palette.highlightedText: "#FFFFFF"
                                }
                            }
                            Text {
                                Layout.leftMargin: 10
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                                text: "\u03C4"
                                color: "#CCCCCC"
                                font: title_font
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: "Biases"
                            color: "#AAAAAA"
                            font: title_font
                        }
                        Repeater {
                            model: parameters.biases_names
                            RowLayout {
                                Layout.topMargin: 5
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                                spacing: 20
                                SpinBox {
                                    palette.button: "#393939"
                                    palette.buttonText: "#FFFFFF"
                                    palette.text: "#FFFFFF"
                                    palette.base: "#191919"
                                    palette.mid: "#494949"
                                    palette.highlight: "#1E88E5"
                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                                    from: 0
                                    to: 255
                                    editable: true
                                    value: parameters[modelData]
                                    onValueModified: {
                                        parameters[modelData] = value;
                                    }
                                }
                                Text {
                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                                    text: modelData
                                    color: "#CCCCCC"
                                    font: title_font
                                }
                            }
                        }
                    }
                }
            }
        }
        RoundButton {
            text: parameters.recording_name ? "\uE047" : "\uE061"
            palette.button: "#393939"
            palette.buttonText: "#FFFFFF"
            x: window.width - 100
            y: 10
            font: icons_font
            icon.width: width
            icon.height: height
            onClicked: {
                if (parameters.recording_name) {
                    parameters.recording_stop_required = requestIndex;
                } else {
                    parameters.recording_start_required = requestIndex;
                }
                ++requestIndex;
            }
        }
        RoundButton {
            text: showMenu ? "\uE5CD" : "\uE8B8"
            palette.button: "#393939"
            palette.buttonText: "#FFFFFF"
            x: window.width - 50
            y: 10
            font: icons_font
            icon.width: width
            icon.height: height
            onClicked: {
                showMenu = !showMenu
            }
        }
    }
)"
