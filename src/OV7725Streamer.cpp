
#include "OV7725Streamer.h"
#include <assert.h>



OV7725Streamer::OV7725Streamer(SOCKET aClient, OV7725aiThinker &cam) : CStreamer(aClient, cam.getWidth(), cam.getHeight()), m_cam(cam)
{
    printf("Created streamer width=%d, height=%d\n", cam.getWidth(), cam.getHeight());
}

void OV7725Streamer::streamImage(uint32_t curMsec)
{
    ledcWrite(0, 250);
    m_cam.run();// queue up a read for next time
    BufPtr bytes = m_cam.getfb();
    printf("streamFrame(), size: %d \n", m_cam.getSize());
    ledcWrite(0, 200);
    streamFrame(bytes, m_cam.getSize(), curMsec);

}
