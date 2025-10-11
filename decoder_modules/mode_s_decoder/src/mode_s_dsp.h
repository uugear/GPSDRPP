#pragma once
#include <dsp/block.h>
#include <dsp/hier_block.h>
#include "mode_s_decoder.h"

namespace dsp {

    uint16_t mag[16641];

    class ModeSBlock : public block {
    public:
        ModeSBlock() {}

        ModeSBlock(mode_s_context * ctx, stream<complex_t>* in) { init(ctx, in); }

        void init(mode_s_context * ctx, stream<complex_t>* in) {
            if (!ctx) return;

            _ctx = ctx;
            _in = in;
            block::registerInput(_in);
            //block::registerOutput(&out);
            block::_block_init = true;

            output_buffer.reserve(10240);
            m_buffer.reserve(5120);


            for (int i = 0; i <= 128; i++) {
                for (int q = 0; q <= 128; q++) {
                    mag[i*129+q] = round(sqrt(i*i+q*q)*360);
                }
            }
        }

        void setInput(stream<complex_t>* in) {
            assert(block::_block_init);
            std::lock_guard<std::recursive_mutex> lck(block::ctrlMtx);
            block::tempStop();
            block::unregisterInput(_in);
            _in = in;
            block::registerInput(_in);
            block::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            if (!running) {
                _in->flush();
                return -1;
            }

            // Convert values to 8-bit integer
			int byteCount = count * 2;
            output_buffer.resize(byteCount);
            int8_t* output_buf = output_buffer.data();
            volk_32f_s32f_convert_8i(output_buf, (float*)_in->readBuf, 128.0f, byteCount);

            m_buffer.resize(count);
            uint16_t * m_buf = m_buffer.data();

            for (int k = 0; k < count; k ++) {
                int i = output_buf[k * 2];
                int q = output_buf[k * 2 + 1];
                if (i < 0) i = -i;
                if (q < 0) q = -q;
                m_buf[k] = mag[i * 129 + q];
            }

            detectModeS(_ctx, m_buf, count);

            _in->flush();
            return count;
        }

        //stream<uint8_t> out;

    private:
        struct mode_s_context * _ctx;
        stream<complex_t>* _in;

        std::vector<int8_t> output_buffer;
        std::vector<uint16_t> m_buffer;
    };


    class ModeSDecoder : public hier_block {
    public:
        ModeSDecoder() {}

        ModeSDecoder(struct mode_s_context * ctx, stream<complex_t>* input) {
            init(ctx, input);
        }

        void init(struct mode_s_context * ctx, stream<complex_t>* input) {
            if (!ctx) return;

            _ctx = ctx;

            mode_s_block.init(_ctx, input);

            hier_block::registerBlock(&mode_s_block);

            hier_block::_block_init = true;
        }

        void setInput(stream<complex_t>* input) {
            assert(hier_block::_block_init);
            mode_s_block.setInput(input);
        }

    private:
        struct mode_s_context * _ctx;
        ModeSBlock mode_s_block;

    };
}