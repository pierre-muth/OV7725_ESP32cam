#pragma once

#include "CStreamer.h"
#include "OV7725aiThinker.h"

class OV7725Streamer : public CStreamer
{
    bool m_showBig;
    OV7725aiThinker &m_cam;

public:
    OV7725Streamer(SOCKET aClient, OV7725aiThinker &cam);

    virtual void    streamImage(uint32_t curMsec);
};
