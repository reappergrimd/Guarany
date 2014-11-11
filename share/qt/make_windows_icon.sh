#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/Guarany.ico

convert ../../src/qt/res/icons/Guarany-16.png ../../src/qt/res/icons/Guarany-32.png ../../src/qt/res/icons/Guarany-48.png ${ICON_DST}
