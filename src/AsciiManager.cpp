#include "AsciiManager.h"

AsciiManager* g_asciiManager;

void AsciiManager::loadAsciiStrings(const char* str, D3DXVECTOR3* position)
{
    if (m_numStrings >= STRING_ARRAY_SIZE)
        return;

    AsciiString* asciiString = &m_strings[m_numStrings++];
    strcpy(asciiString->text, str);
    asciiString->pos = *position;
    asciiString->color = m_color;
    asciiString->scale = scale;
    asciiString->alignH = alignH;
    asciiString->fontId = fontId;
    asciiString->drawShadows = drawShadows;
    asciiString->renderGroup = renderGroup;
    asciiString->remainingTime = duration;
}