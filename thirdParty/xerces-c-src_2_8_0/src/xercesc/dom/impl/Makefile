#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
# 
#      http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
#
# $Id: Makefile.in 570129 2007-08-27 14:10:51Z amassari $
#

PLATFORM = LINUX
CC  = gcc
CXX = g++
CXXVER = 
GCC = yes
GXX = yes
CXXFLAGS = -g -O2
CFLAGS = -g -O2
PREFIX = /usr/local
PREFIX_INCLUDE = ${PREFIX}/include/xercesc
LDFLAGS = 
LIBS = 
OSVER = 
USELIBWWW = 
MESSAGELOADER = 
TRANSCODER = 
THREADS = 
LIBTYPE = 
ARCH = x86_64

MODULE = dom
SUBMODULE = impl

include ../../Makefile.incl

DOM_IMPL_CPP_PUBHEADERS =

DOM_IMPL_CPP_PRIVHEADERS =  \
	DOMAttrImpl.hpp \
	DOMAttrMapImpl.hpp \
	DOMAttrNSImpl.hpp \
	DOMCasts.hpp \
	DOMCDATASectionImpl.hpp \
	DOMCharacterDataImpl.hpp \
	DOMChildNode.hpp \
	DOMCommentImpl.hpp \
        DOMConfigurationImpl.hpp \
	DOMDeepNodeListImpl.hpp \
	DOMDeepNodeListPool.hpp \
	DOMDocumentFragmentImpl.hpp \
	DOMDocumentImpl.hpp \
	DOMDocumentTypeImpl.hpp \
	DOMImplementationImpl.hpp \
	DOMElementImpl.hpp \
	DOMElementNSImpl.hpp \
	DOMEntityImpl.hpp \
	DOMEntityReferenceImpl.hpp \
	DOMErrorImpl.hpp \
	DOMLocatorImpl.hpp \
	DOMNamedNodeMapImpl.hpp \
	DOMNodeIDMap.hpp \
	DOMNodeImpl.hpp \
	DOMNodeIteratorImpl.hpp \
	DOMNodeListImpl.hpp \
	DOMNodeVector.hpp \
	DOMNormalizer.hpp \
	DOMNotationImpl.hpp \
	DOMParentNode.hpp \
	DOMProcessingInstructionImpl.hpp \
	DOMRangeImpl.hpp \
	DOMStringPool.hpp \
	DOMTextImpl.hpp \
	DOMTreeWalkerImpl.hpp \
	DOMTypeInfoImpl.hpp \
	DOMWriterImpl.hpp \
	XSDElementNSImpl.hpp

DOM_IMPL_C_FILES = \
	DOMDeepNodeListPool.c


DOM_IMPL_CPP_OBJECTS = \
	DOMAttrImpl.$(TO) \
	DOMAttrMapImpl.$(TO) \
	DOMAttrNSImpl.$(TO) \
	DOMCDATASectionImpl.$(TO) \
	DOMCharacterDataImpl.$(TO) \
	DOMChildNode.$(TO) \
	DOMCommentImpl.$(TO) \
        DOMConfigurationImpl.$(TO) \
	DOMDeepNodeListImpl.$(TO) \
	DOMDocumentFragmentImpl.$(TO) \
	DOMDocumentImpl.$(TO) \
	DOMDocumentTypeImpl.$(TO) \
	DOMImplementationImpl.$(TO) \
	DOMImplementationRegistry.$(TO) \
	DOMElementImpl.$(TO) \
	DOMElementNSImpl.$(TO) \
	DOMEntityImpl.$(TO) \
	DOMEntityReferenceImpl.$(TO) \
	DOMErrorImpl.$(TO) \
	DOMLocatorImpl.$(TO) \
	DOMNamedNodeMapImpl.$(TO) \
	DOMNodeIDMap.$(TO) \
	DOMNodeImpl.$(TO) \
	DOMNodeIteratorImpl.$(TO) \
	DOMNodeListImpl.$(TO) \
	DOMNodeVector.$(TO) \
	DOMNormalizer.$(TO) \
	DOMNotationImpl.$(TO) \
	DOMParentNode.$(TO) \
	DOMProcessingInstructionImpl.$(TO) \
	DOMRangeImpl.$(TO) \
	DOMStringPool.$(TO) \
	DOMTextImpl.$(TO) \
	DOMTreeWalkerImpl.$(TO) \
	DOMTypeInfoImpl.$(TO) \
	DOMWriterImpl.$(TO) \
	XSDElementNSImpl.$(TO)


all::	includes $(DOM_IMPL_CPP_OBJECTS)

includes::	pubheaders $(DOM_IMPL_C_FILES)

pubheaders::
	$Qmkdir -p $(XML_INC_DIR)/$(MODULE)/$(SUBMODULE)
	@echo "  (CP)  $(XML_INC_DIR)/$(MODULE)/$(SUBMODULE)"
	$Q$(CP) $(DOM_IMPL_CPP_PUBHEADERS) $(DOM_IMPL_C_FILES) $(XML_INC_DIR)/$(MODULE)/$(SUBMODULE)

# this may generate unnecessary dependencies, but it makes life easier
depend:: includes
	@echo "  (DEP)"
	$Q$(MAKE_DEPEND) $(XML_INCL)  *.cpp > $(DEPFILE)

clean::
	@echo "Making clean in $(MODULE)/$(SUBMODULE) ..."
	$Q$(RM2) $(addprefix $(XML_OBJ_DIR)/,$(DOM_IMPL_CPP_OBJECTS))

distclean::	clean
	$Q$(RM) Makefile $(DEPFILE)
	@echo "Removing all $(MODULE)/$(SUBMODULE) header files ..."
ifneq ($(strip $(DOM_IMPL_CPP_PUBHEADERS)),)
	$Q$(RM2) $(addprefix $(XML_INC_DIR)/$(MODULE)/$(SUBMODULE)/,$(DOM_IMPL_CPP_PUBHEADERS))
endif

install::
	$Qmkdir -p $(DESTDIR)$(PREFIX_INCLUDE)/$(MODULE)/$(SUBMODULE)
	@echo "  (CP)"
	$Q$(CP) $(DOM_IMPL_CPP_PUBHEADERS) $(DOM_IMPL_C_FILES) $(DESTDIR)$(PREFIX_INCLUDE)/$(MODULE)/$(SUBMODULE)
