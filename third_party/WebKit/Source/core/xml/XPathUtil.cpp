/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/xml/XPathUtil.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/NodeTraversal.h"
#include "wtf/text/StringBuilder.h"

namespace blink {
namespace XPath {

bool isRootDomNode(Node* node)
{
    return node && !node->parentNode();
}

String stringValue(Node* node)
{
    switch (node->getNodeType()) {
    case Node::kAttributeNode:
    case Node::kProcessingInstructionNode:
    case Node::kCommentNode:
    case Node::kTextNode:
    case Node::kCdataSectionNode:
        return node->nodeValue();
    default:
        if (isRootDomNode(node) || node->isElementNode()) {
            StringBuilder result;
            result.reserveCapacity(1024);

            for (Node& n : NodeTraversal::descendantsOf(*node)) {
                if (n.isTextNode()) {
                    const String& nodeValue = n.nodeValue();
                    result.append(nodeValue);
                }
            }

            return result.toString();
        }
    }

    return String();
}

bool isValidContextNode(Node* node)
{
    if (!node)
        return false;
    switch (node->getNodeType()) {
    case Node::kAttributeNode:
    case Node::kCdataSectionNode:
    case Node::kCommentNode:
    case Node::kDocumentNode:
    case Node::kElementNode:
    case Node::kProcessingInstructionNode:
        return true;
    case Node::kDocumentFragmentNode:
    case Node::kDocumentTypeNode:
        return false;
    case Node::kTextNode:
        return !(node->parentNode() && node->parentNode()->isAttributeNode());
    }
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace XPath
} // namespace blink
