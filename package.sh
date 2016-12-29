#!/bin/sh

python "$EMSCRIPTEN/tools/file_packager.py" game.data --preload "$1"@/ --js-output=game.js
