Initial D Tools
===============
These tools can aid in modifying assets in Initial D Special Stage. It's being used to help translate the game into English.

Resource Files
--------------
The game stores files in fairly simple file formats and doesn't use compression for anything other than audio. ADX files are used to store resources (textures, audio, 3D models, etc). PAC files are stored within the ADX files and contain textures and other assets. Textures are stored in .GIM files using either 8bpp (256 color) or 4bpp (16 color) format. Both formats use a 32-bit RGBA palette with a 0-128 range alpha channel. Most of the 4bpp textures follow the same format, but some are stored differently in a format I don’t understand. This doesn't really matter because the images that need to be translated are all in the normal 4bpp format as far as I can tell. 8bpp images are swizzled and must be de-swizzled before being converted to PNG. 8bpp images also have weird palettes that have to be filtered using a special algorithm (admittedly I don’t understand the algorithm, but the palette is stored this way to improve performance in-game). Japanese text can be found in text files, but a conversion table will likely be needed to read this since it isn’t encoded using standard shift-jis. I haven’t looked into the text yet.

Conversion Tools
----------------
gimtool to convert .gim textures and pactool can extract/create .pac files. Since ADX files are used in other games, extraction and creation tools already exist and are compatible with this game. gimtool will handle gim files by converting them to and from standard PNG files that can be edited using standard image editing software. The extractor has been tested with 8bpp textures and most 4bpp textures, and texture replacement is working as expected too. It's much easier to inject a PNG rather than create the whole GIM structure from scratch since I don’t fully understand the format. When injecting a PNG, it will be palettized to fit either a 16 or 256 color RBGA palette depending on what type the target GIM uses.

pactool will handle .PAC extraction and building. Extracting and building is fully functional and has been tested by repacking several .PAC files. The game was able to load re-packed .PAC files without any issues :).