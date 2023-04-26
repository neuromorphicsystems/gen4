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
                dvs_display.trigger_draw();
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
                Rectangle {
                    id: overlay
                    color: "transparent"
                    border.color: "#dc533d"
                    border.width: 0
                }
                DvsDisplay {
                    objectName: "dvs_display"
                    id: dvs_display
                    width: eventsView.width
                    height: eventsView.height
                    canvas_size: Qt.size(header_width, header_height)
                    parameter: 100000
                    style: DvsDisplay.Linear
                    on_colormap: ['#F4C20D', '#191919']
                    off_colormap: ['#1E88E5', '#191919']
                    onPaintAreaChanged: {
                        overlay.x = paint_area.x / Screen.devicePixelRatio
                        overlay.y = paint_area.y / Screen.devicePixelRatio
                        overlay.width = paint_area.width / Screen.devicePixelRatio
                        overlay.height = paint_area.height / Screen.devicePixelRatio
                    }
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
                            font.pointSize: 16
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
                            font.pointSize: 16
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
                            font.pointSize: 16
                        }
                        Text {
                            Layout.bottomMargin: 20
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: parameters.event_rate
                            color: "#FFFFFF"
                            font: monospace_font;
                        }

                        // serial
                        Text {
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: "Serial"
                            color: "#AAAAAA"
                            font.pointSize: 16
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
                            font.pointSize: 16
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
                                    font.pointSize: 16
                                }
                            }
                        }
                        RowLayout {
                            Layout.topMargin: 5
                            Layout.bottomMargin: 10
                            ComboBox {
                                id: style_combo_box
                                model: ["Exponential", "Linear", "Window"]
                                currentIndex: 0
                                onCurrentIndexChanged: {
                                    dvs_display.style = [DvsDisplay.Exponential, DvsDisplay.Linear, DvsDisplay.Window][currentIndex]
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
                                    font.weight: style_combo_box.currentIndex === index ? Font.Bold : Font.Normal
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
                                font.pointSize: 16
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
                                    dvs_display.parameter = [50, 100, 200, 500, 1000, 5000, 10000, 50000, 100000][currentIndex] * 1000
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
                                    font.weight: tau_combo_box.currentIndex === index ? Font.Bold : Font.Normal
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
                                font.pointSize: 16
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: "Biases"
                            color: "#AAAAAA"
                            font.pointSize: 16
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
                                    font.pointSize: 16
                                }
                            }
                        }
                    }
                }
            }
        }
        RoundButton {
            text: parameters.recording_name ? "\u25FE" : "\u25CF"
            palette.button: "#393939"
            palette.buttonText: "#FFFFFF"
            x: window.width - 100
            y: 10
            font.family: "Arial"
            font.pointSize: 24
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
            text: showMenu ? "\u00D7" : "\u2699"
            palette.button: "#393939"
            palette.buttonText: "#FFFFFF"
            x: window.width - 50
            y: 10
            font.family: "Arial"
            font.pointSize: 24
            icon.width: width
            icon.height: height
            onClicked: {
                showMenu = !showMenu
            }
        }
    }
)"
