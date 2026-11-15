import QtQuick
import QtLocation
import QtPositioning

Item {
    width: 256; height: 256
    Map {
        id: map
        objectName: "map"
        anchors.fill: parent
        center: QtPositioning.coordinate(0, 0)
        zoomLevel: 3
        plugin: Plugin {
            name: "maplibre"
            PluginParameter { name: "maplibre.map.styles"; value: testStyleUrl }
        }
    }
}
