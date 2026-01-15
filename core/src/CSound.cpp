#include "CSound.h"
#include "Globals.h"

CSound::CSound(IDirectSoundBuffer** apDSBuffer, DWORD dwDSBufferSize, CWaveFile* pWaveFile)
{
    m_apDSBuffer = (IDirectSoundBuffer**)game_new(4);
    *m_apDSBuffer = *apDSBuffer;
    m_dwDSBufferSize = dwDSBufferSize;
    m_dwNumBuffers = 1;
    m_WaveFile = pWaveFile;
    fillBufferWithSound(this, *m_apDSBuffer, 0);
    (*m_apDSBuffer)->SetCurrentPosition(0);
    m_dsBufferDesc.lpwfxFormat = 0;
    m_dsBufferDesc.guid3DAlgorithm.Data1 = 0;
}

// 0x45a6f0
HRESULT CSound::fillBufferWithSound(CSound* This, LPDIRECTSOUNDBUFFER pDSBuffer, BOOL bRepeatWavIfBufferLarger)
{
    return S_OK;
}