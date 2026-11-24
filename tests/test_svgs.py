import xml.etree.ElementTree as ElementTree
from pathlib import Path


SVGS = [
    "black/close.svg",
    "black/line.svg",
    "black/marker.svg",
    "black/mouse.svg",
    "black/name_edition.svg",
    "black/pencil.svg",
    "black/size_indicator.svg",
    "black/square-outline.svg",
    "black/square.svg",
    "black/text.svg",
    "black/undo-variant.svg",
    "white/close.svg",
    "white/line.svg",
    "white/marker.svg",
    "white/mouse.svg",
    "white/name_edition.svg",
    "white/pencil.svg",
    "white/size_indicator.svg",
    "white/square-outline.svg",
    "white/square.svg",
    "white/text.svg",
    "white/undo-variant.svg",
]


def main():
    base = Path(__file__).resolve().parents[1] / "data" / "img" / "material"
    for relative_path in SVGS:
        root = ElementTree.parse(base / relative_path).getroot()
        assert root.tag == "{http://www.w3.org/2000/svg}svg", relative_path
    marker = ElementTree.parse(base / "white" / "size_indicator.svg").getroot()
    assert marker[0].attrib["fill"] == "none"


if __name__ == "__main__":
    main()
