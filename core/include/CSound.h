#pragma once
#include "Chireiden.h"
#include "CWaveFile.h"
#include "Macros.h"

class CSoundManager;
class CSound
{
public:
    void* vtable;
    IDirectSoundBuffer** m_apDSBuffer;
    DWORD m_dwDSBufferSize;
    CWaveFile* m_WaveFile;
    DWORD m_dwNumBuffers;
    int currentFadeProgress;
    int m_totalFade;
    BOOL isPlaying;
    DSBUFFERDESC m_dsBufferDesc;
    CSoundManager* m_pSoundManager;
    int e[4];

    CSound(IDirectSoundBuffer** apDSBuffer, DWORD dwDSBufferSize, CWaveFile* pWaveFile);

    static HRESULT fillBufferWithSound(CSound* This, LPDIRECTSOUNDBUFFER pDSBThenBufferSize, BOOL bRepeatWavIfBufferLarger);

};
ASSERT_SIZE(CSound, 0x58); // VERIFY WHAT MEMBERS ARE INSIDE CSOUND AND NOT CSTREAMINGSOUND