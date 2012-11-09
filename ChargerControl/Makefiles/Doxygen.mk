#
# embedXcode
# ----------------------------------
# Embedded Computing on Xcode 4
#
# Copyright © Rei VILO, 2010-2012
# Licence CC = BY NC SA
#

# References and contribution
# ----------------------------------
# See About folder
#
# www.doxygen.org
# 	© 1997-2012 by Dimitri van Heesch
# 	GNU General Public License
# 	Documents produced by doxygen are derivative works derived from the input used in their production.
# 	They are not affected by this license.
#

# Doxygen specifics
# ----------------------------------
# Edit ./Documents/doxyfile for options
#

$(info ---- doxygen ----)

DOXYGEN_PATH = /Applications/Doxygen.app/Contents/Resources/doxygen
ifeq ($(wildcard $(DOXYGEN_PATH)),)
    $(error Error: application doxygen not found)
endif

DOCUMENTS_PATH = $(CURDIR)/Utilities
DOXYFILE       = $(DOCUMENTS_PATH)/doxyfile
ifeq ($(wildcard $(DOXYFILE)),)
    $(error Error: configuration file doxyfile not found)
endif


GENERATE_HTML   = $(strip $(shell grep 'GENERATE_HTML *=' $(DOXYFILE) | cut -d = -f 2 ))
GENERATE_PDF    = $(strip $(shell grep 'GENERATE_LATEX *=' $(DOXYFILE) | cut -d = -f 2 ))
GENERATE_DOCSET = $(strip $(shell grep 'GENERATE_DOCSET *=' $(DOXYFILE) | cut -d = -f 2 ))

BUNDLE_ID       = $(strip $(shell grep 'DOCSET_BUNDLE_ID *=' $(DOXYFILE) | cut -d = -f 2 ))
DOCSET_PATH     = $(USER_PATH)/Library/Developer/Shared/Documentation/DocSets/$(BUNDLE_ID).docset
LOAD_UTIL_PATH  = $(UTILITIES_PATH)/loadDocSet.scpt

PDF_PATH        = $(DOCUMENTS_PATH)/latex/refman.pdf
TEX_PATH        = $(DOCUMENTS_PATH)/latex/refman.tex

$(info .    html       	$(GENERATE_HTML))
$(info .    pdf        	$(GENERATE_PDF))
$(info .    docset  	$(GENERATE_DOCSET))
$(info .    bundle id  	$(BUNDLE_ID))


ifeq ($(GENERATE_HTML),YES)
ifeq ($(wildcard $(DOXYGEN_PATH)),)
    $(error Error: application doxygen not found)
endif
endif

#embed1
#echo $VAR | tr '[:upper:]' '[:lower:]' | sed 's/ //g'




