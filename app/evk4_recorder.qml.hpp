R"(
    import Chameleon 1.0
    import QtQuick 2.7
    import QtQuick.Layouts 1.2
    import QtQuick.Window 2.2
    import QtQuick.Controls 2.1
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
        BackgroundCleaner {
            width: window.width
            height: window.height
            color: '#292929'
        }
        Rectangle {
            id: overlay
            color: "transparent"
            border.color: "#dc533d"
            border.width: 0
        }
        ColumnLayout {
            width: window.width
            height: window.height
            spacing: 0
            DvsDisplay {
                objectName: "dvs_display"
                id: dvs_display
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                canvas_size: Qt.size(header_width, header_height)
                parameter: 200000
                style: DvsDisplay.Linear
                on_colormap: ['#f4c20d', '#393939']
                off_colormap: ['#1e88e5', '#393939']
                onPaintAreaChanged: {
                    overlay.x = paint_area.x / Screen.devicePixelRatio
                    overlay.y = paint_area.y / Screen.devicePixelRatio
                    overlay.width = paint_area.width / Screen.devicePixelRatio
                    overlay.height = paint_area.height / Screen.devicePixelRatio
                }
            }
        }
    }
)"
