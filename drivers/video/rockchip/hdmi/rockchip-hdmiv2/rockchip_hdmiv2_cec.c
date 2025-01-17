#include <linux/delay.h>
#include "../rockchip-hdmi-cec.h"
#include "rockchip_hdmiv2.h"
#include "rockchip_hdmiv2_hw.h"

/* static wait_queue_head_t	wait;*/
static int init = 1;
void rockchip_hdmiv2_cec_isr(struct hdmi_dev *hdmi_dev, char cec_int)
{
	CECDBG("%s cec 0x%x\n", __func__, cec_int);
	if (cec_int & m_EOM)
		rockchip_hdmi_cec_submit_work(EVENT_RX_FRAME, 0, NULL);
	if (cec_int & m_DONE)
		CECDBG("send frame success\n");
}

static int rockchip_hdmiv2_cec_readframe(struct hdmi *hdmi,
					 struct cec_framedata *frame)
{
	struct hdmi_dev *hdmi_dev = hdmi->property->priv;
	int i, count;
	char *data = (char *)frame;

	if (frame == NULL)
		return -1;
	count = hdmi_readl(hdmi_dev, CEC_RX_CNT);
	CECDBG("%s count %d\n", __func__, count);
	for (i = 0; i < count; i++) {
		data[i] = hdmi_readl(hdmi_dev, CEC_RX_DATA0 + i);
		CECDBG("%02x\n", data[i]);
	}
	hdmi_writel(hdmi_dev, CEC_LOCK, 0x0);
	return 0;
}


void rockchip_hdmiv2_cec_setcecla(struct hdmi *hdmi, int ceclgaddr)
{
	struct hdmi_dev *hdmi_dev = hdmi->property->priv;
	short val;

	if (ceclgaddr < 0 || ceclgaddr > 16)
		return;
	val = 1 << ceclgaddr;
	hdmi_writel(hdmi_dev, CEC_ADDR_L, val & 0xff);
	hdmi_writel(hdmi_dev, CEC_ADDR_H, val>>8);
}

static int rockchip_hdmiv2_cec_sendframe(struct hdmi *hdmi,
					 struct cec_framedata *frame)
{
	struct hdmi_dev *hdmi_dev = hdmi->property->priv;
	int i, interrupt;

	CECDBG("TX srcdestaddr %02x opcode %02x ",
	       frame->srcdestaddr, frame->opcode);
	if (frame->argcount) {
		CECDBG("args:");
		for (i = 0; i < frame->argcount; i++)
			CECDBG("%02x ", frame->args[i]);
	}
	CECDBG("\n");
	if ((frame->srcdestaddr & 0x0f) == ((frame->srcdestaddr >> 4) & 0x0f)) {
		/*it is a ping command*/
		hdmi_writel(hdmi_dev, CEC_TX_DATA0, frame->srcdestaddr);
		hdmi_writel(hdmi_dev, CEC_TX_CNT, 1);
	} else {
		hdmi_writel(hdmi_dev, CEC_TX_DATA0, frame->srcdestaddr);
		hdmi_writel(hdmi_dev, CEC_TX_DATA0 + 1, frame->opcode);
		for (i = 0; i < frame->argcount; i++)
			hdmi_writel(hdmi_dev,
				    CEC_TX_DATA0 + 2 + i, frame->args[i]);
		hdmi_writel(hdmi_dev, CEC_TX_CNT, frame->argcount + 2);
	}
	/*Start TX*/
	hdmi_msk_reg(hdmi_dev, CEC_CTRL, m_CEC_SEND, v_CEC_SEND(1));
	i = 20;
	while (i--) {
		usleep_range(900, 1000);
		interrupt = hdmi_readl(hdmi_dev, IH_CEC_STAT0);
		if (interrupt & (m_ERR_INITIATOR | m_ARB_LOST |
					m_NACK | m_DONE)) {
			hdmi_writel(hdmi_dev, IH_CEC_STAT0,
				    interrupt & (m_ERR_INITIATOR |
				    m_ARB_LOST | m_NACK | m_DONE));
			break;
		}
	}
	CECDBG("%s interrupt 0x%02x\n", __func__, interrupt);
	if (interrupt & m_DONE)
		return 0;
	else if (interrupt & m_NACK)
		return 1;
	else
		return  -1;
}

void rockchip_hdmiv2_cec_init(struct hdmi *hdmi)
{
	struct hdmi_dev *hdmi_dev = hdmi->property->priv;

	if (init) {
		rockchip_hdmi_cec_init(hdmi,
				       rockchip_hdmiv2_cec_sendframe,
				       rockchip_hdmiv2_cec_readframe,
				       rockchip_hdmiv2_cec_setcecla);
		init = 0;
		/* init_waitqueue_head(&wait); */
	}
	hdmi_writel(hdmi_dev, IH_MUTE_CEC_STAT0, m_ERR_INITIATOR |
			m_ARB_LOST | m_NACK | m_DONE);
	CECDBG("%s", __func__);
}
