import dataclasses


@dataclasses.dataclass
class Theme:
    background: str
    on: str
    off: str
    axis: str


name_to_theme: dict[str, Theme] = {
    "WSU": Theme(
        background="#000000FF", on="#82b4c8A0", off="#660022A0", axis="#800000FF"
    ),
    "Starry Night": Theme(
        background="#111111FF", on="#F4B400A0", off="#4285F4A0", axis="#777777FF"
    ),
    "Guernica": Theme(
        background="#111111FF", on="#CCCCCCA0", off="#555555A0", axis="#777777FF"
    ),
    "The Son of Man": Theme(
        background="#202020FF", on="#688642A0", off="#883125A0", axis="#777777FF"
    ),
}

names = list(name_to_theme.keys())
values = list(name_to_theme[name] for name in names)
default = name_to_theme.values().__iter__().__next__()
