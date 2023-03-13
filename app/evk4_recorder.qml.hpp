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
                    parameter: 200000
                    style: DvsDisplay.Linear
                    on_colormap: ['#f4c20d', '#191919']
                    off_colormap: ['#1e88e5', '#191919']
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
                        spacing: 10

                        Text {
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                            text: "Recordings directory: ./recordings"
                            color: "#FFFFFF"
                        }

                        Repeater {
                            model: [
                                "pr",
                                "fo",
                                "hpf",
                                "diff_on",
                                "diff",
                                "diff_off",
                                "inv",
                                "refr",
                                "reqpuy",
                                "reqpux",
                                "sendreqpdy",
                                "unknown_1",
                                "unknown_2",
                            ]
                            RowLayout {
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                                spacing: 20
                                Text {
                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                                    text: modelData
                                    color: "#FFFFFF"
                                }
                                SpinBox {
                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                                    from: 0
                                    to: 255
                                }
                            }
                        }
                    }
                }
            }
        }
        RoundButton {
            text: showMenu ? "\u00D7" : "\u2699"
            palette.button: "#393939"
            palette.buttonText: "#ffffff"
            x: window.width - 50
            y: 10
            font.family: "Arial"
            font.pointSize: 24
            onClicked: {
                showMenu = !showMenu
            }
        }
    }
)"
