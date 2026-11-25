#pragma once
/*
Real-time centre/side splitter
------------------------------
STFT overlap-add (Hann analysis * Hann synthesis, 75% overlap) splits stereo into:
- pce   – stereo mask extracted  (mono centre)
- pcrL  – stereo mask removed L  (side residual, left)
- pcrR  – stereo mask removed R  (side residual, right)
*/

#include <cmath>
#include <vector>

class StereoMaskEngine {
	public:
		static constexpr int    FFT     = 4096;
		static constexpr double overlap = 0.75;
		static constexpr int    LATENCY = FFT; // kept for Processor compat

		StereoMaskEngine() = default;

		// Rebuild all state for a new sample rate / FFT size.
		// fftSize is clamped to the nearest power of two in [2048, 16384].
		void prepare(double sampleRate, int fftSize = FFT) {
			fftN    = nearestPow2(fftSize, 2048, 16384);
			hop     = static_cast<int>(fftN * (1.0 - overlap) + 0.5);
			bins    = fftN / 2 + 1;
			bits    = log2i(fftN);
			ring    = nextPow2(fftN * 2);
			ola     = nextPow2(fftN * 2);
			latency = fftN;

			window .assign(fftN,     0.f);
			twRe   .assign(fftN / 2, 0.f);
			twIm   .assign(fftN / 2, 0.f);
			rev    .assign(fftN,     0);

			inL.assign(ring, 0.f);  inR.assign(ring, 0.f);

			pceBuf .assign(ola, 0.f);
			pcrLBuf.assign(ola, 0.f);
			pcrRBuf.assign(ola, 0.f);

			smoothPow .assign(bins * 2, 0.f);
			smoothMask.assign(bins, 0.5f);
			rawMask   .assign(bins, 0.f);

			xr.assign(fftN, 0.f); xi.assign(fftN, 0.f);
			lr.assign(fftN, 0.f); li.assign(fftN, 0.f);
			rr.assign(fftN, 0.f); ri.assign(fftN, 0.f);
			ar.assign(fftN, 0.f); ai.assign(fftN, 0.f);
			br.assign(fftN, 0.f); bi.assign(fftN, 0.f);

			buildTables();
			setSmoothing(sampleRate);
			reset();
		}

		void reset() {
			std::fill(inL.begin(),  inL.end(),  0.f);
			std::fill(inR.begin(),  inR.end(),  0.f);

			std::fill(pceBuf.begin(),  pceBuf.end(),  0.f);
			std::fill(pcrLBuf.begin(), pcrLBuf.end(), 0.f);
			std::fill(pcrRBuf.begin(), pcrRBuf.end(), 0.f);

			std::fill(smoothPow.begin(),  smoothPow.end(),  0.f);
			std::fill(smoothMask.begin(), smoothMask.end(), 0.5f);

			inPos    = 0;
			readPos  = 0;
			hopCount = hop;
		}

		int getLatency() const {
			return latency;
		}

		void process(
			float l,
			float r,

			float& pce,
			float& pcrL,
			float& pcrR
		) {
			inL[inPos] = l;
			inR[inPos] = r;
			inPos = (inPos + 1) & (ring - 1);

			if (--hopCount <= 0) {
				processHop();
				hopCount = hop;
			}

			pce  = pceBuf[readPos];
			pceBuf[readPos]  = 0.f;

			pcrL = pcrLBuf[readPos];
			pcrLBuf[readPos] = 0.f;

			pcrR = pcrRBuf[readPos];
			pcrRBuf[readPos] = 0.f;

			readPos = (readPos + 1) & (ola - 1);
		}

	private:
		int fftN    = FFT;
		int hop     = static_cast<int>(FFT * (1.0 - overlap) + 0.5);
		int bins    = FFT / 2 + 1;
		int bits    = 9;
		int ring    = 2048;
		int ola     = 2048;
		int latency = FFT;

		// Utility
		static int log2i(int n) {
			int b = 0;

			while ((1 << b) < n) {
				++b;
			}

			return b;
		}

		static int nextPow2(int n) {
			int p = 1;

			while (p < n) {
				p <<= 1;
			}

			return p;
		}

		static int nearestPow2(int n, int lo, int hi) {
			n = nextPow2(n);

			if (n < lo) {
				n = lo;
			}

			if (n > hi) {
				n = hi;
			}

			return n;
		}

		void buildTables() {
			constexpr double twoPi = juce::MathConstants<double>::twoPi;

			for (int i = 0; i < fftN; ++i) {
				window[i] = (float)(0.5 - 0.5 * std::cos(twoPi * i / fftN));
			}

			for (int i = 0; i < fftN / 2; ++i) {
				twRe[i] = (float) std::cos(twoPi * i / fftN);
				twIm[i] = (float)-std::sin(twoPi * i / fftN);
			}

			for (int i = 0; i < fftN; ++i) {
				int r = 0;

				for (int b = 0; b < bits; ++b) {
					if (i & (1 << b)) {
						r |= 1 << (bits - 1 - b);
					}
				}

				rev[i] = r;
			}
		}

		void setSmoothing(double sampleRate) {
			const double hr = sampleRate / hop; // hops per second

			powCoef  = (float)std::exp(-1.0 / (0.008 * hr)); // ~8 ms
			maskCoef = (float)std::exp(-1.0 / (0.003 * hr)); // ~3 ms
		}

		void fft(std::vector<float>& re, std::vector<float>& im, bool inverse) {
			for (int i = 0; i < fftN; ++i) {
				const int j = rev[i];

				if (j > i) {
					std::swap(re[i], re[j]); std::swap(im[i], im[j]);
				}
			}
			for (int len = 2; len <= fftN; len <<= 1) {
				const int half = len >> 1, step = fftN / len;

				for (int i = 0; i < fftN; i += len) {
					for (int k = 0; k < half; ++k) {
						const float wr = twRe[k * step];
						const float wi = inverse ? -twIm[k * step] : twIm[k * step];

						const int a = i + k, b = a + half;

						const float tr = re[b] * wr - im[b] * wi;
						const float ti = re[b] * wi + im[b] * wr;

						re[b] = re[a] - tr;
						im[b] = im[a] - ti;

						re[a] += tr;
						im[a] += ti;
					}
				}
			}

			if (inverse) {
				const float s = 1.0f / fftN;

				for (int i = 0; i < fftN; ++i) {
					re[i] *= s;
					im[i] *= s;
				}
			}
		}

		// Enforce Hermitian symmetry: X[N-k] = conj(X[k]) for k=1..N/2-1
		// and zero the imaginary parts of DC (k=0) and Nyquist (k=N/2).
		void enforceHermitian(std::vector<float>& re, std::vector<float>& im) {
			re[0] = re[0]; im[0] = 0.f;
			im[fftN / 2] = 0.f;

			for (int k = 1; k < fftN / 2; ++k) {
				const int m = fftN - k;
				re[m] =  re[k];
				im[m] = -im[k];
			}
		}

		void processHop() {
			// window and forward FFT (complex trick: L -> real, R -> imag)
			const int start = (inPos - fftN + ring) & (ring - 1);
			for (int i = 0; i < fftN; ++i) {
				const int idx = (start + i) & (ring - 1);
				xr[i] = inL[idx] * window[i];
				xi[i] = inR[idx] * window[i];
			}
			fft(xr, xi, false);

			// split into L/R spectra
			for (int k = 0; k < fftN; ++k) {
				const int n = (fftN - k) & (fftN - 1);

				lr[k] =  0.5f * (xr[k] + xr[n]);
				li[k] =  0.5f * (xi[k] - xi[n]);

				rr[k] =  0.5f * (xi[k] + xi[n]);
				ri[k] = -0.5f * (xr[k] - xr[n]);
			}

			// build centre-dominance mask (positive frequencies only)
			for (int k = 0; k < bins; ++k) {
				// M/S
				const float mr = 0.5f * (lr[k] + rr[k]), mi = 0.5f * (li[k] + ri[k]);
				const float sr = 0.5f * (lr[k] - rr[k]), si = 0.5f * (li[k] - ri[k]);

				const float midP  = mr*mr + mi*mi;
				const float sideP = sr*sr + si*si;

				// IIR-smooth the per-bin power
				float& sm = smoothPow[k];
				float& ss = smoothPow[bins + k];

				sm = sm * powCoef + midP  * (1.f - powCoef);
				ss = ss * powCoef + sideP * (1.f - powCoef);

				// wiener-like mask: (mid / (mid+side))^2
				const float m = sm / (sm + ss + 1e-12f);
				rawMask[k] = m * m;
			}

			// 5-tap box frequency smoother
			for (int k = 0; k < bins; ++k) {
				float f = 0.f;
				int   n = 0;

				for (int d = -2; d <= 2; ++d) {
					const int j = k + d;

					if (j >= 0 && j < bins) {
						f += rawMask[j]; ++n;
					}
				}

				f /= (float)n;

				smoothMask[k] = smoothMask[k] * maskCoef + f * (1.f - maskCoef);
			}

			// apply mask to full spectrum and pack into two complex arrays
			// ar + j*ai  carries the pce  signal (centre)
			// br + j*bi  carries pcrL (real) and pcrR (imag) via the same complex trick
			for (int k = 0; k < fftN; ++k) {
				const float m = smoothMask[k < bins ? k : fftN - k];

				const float midR =  0.5f * (lr[k] + rr[k]);
				const float midI =  0.5f * (li[k] + ri[k]);
				const float sidR =  0.5f * (lr[k] - rr[k]);
				const float sidI =  0.5f * (li[k] - ri[k]);

				// Centre: mid * mask
				ar[k] = midR * m;
				ai[k] = midI * m;

				// Residual: remaining mid + full side
				const float rm  = 1.f - m;
				const float pmR = midR * rm, pmI = midI * rm;

				// pcrL = (mid_residual + side), pcrR = (mid_residual - side)
				// packed as IFFT(pcrL + j·pcrR) -> real=pcrL, imag=pcrR
				br[k] = (pmR + sidR) - (pmI - sidI); // real(pcrL) - imag(pcrR)
				bi[k] = (pmI + sidI) + (pmR - sidR); // imag(pcrL) + real(pcrR)
			}

			// enforce Hermitian symmetry so IFFTs yield real outputs
			enforceHermitian(ar, ai);

			fft(ar, ai, true);
			fft(br, bi, true);

			// OLA to ring buffers
			const float colaSum = 0.375f * static_cast<float>(fftN) / static_cast<float>(hop);
			const float gain    = 1.0f / colaSum;

			for (int i = 0; i < fftN; ++i) {
				const int   pos = (readPos + 1 + i) & (ola - 1);
				const float w   = window[i] * gain;

				pceBuf [pos] += ar[i] * w;
				pcrLBuf[pos] += br[i] * w;
				pcrRBuf[pos] += bi[i] * w;
			}
		}

		// Tables
		std::vector<float> window, twRe, twIm;
		std::vector<int>   rev;

		// State
		std::vector<float> inL, inR;
		std::vector<float> pceBuf, pcrLBuf, pcrRBuf;

		std::vector<float> smoothPow;  // [0..bins-1]=mid power, [bins..2bins-1]=side power
		std::vector<float> smoothMask;
		std::vector<float> rawMask;    // scratch for the mask, sized to bins

		// FFT workspace
		std::vector<float> xr, xi, lr, li, rr, ri, ar, ai, br, bi;

		float powCoef  = 0.9f;
		float maskCoef = 0.7f;
		int   inPos    = 0;
		int   readPos  = 0;
		int   hopCount = static_cast<int>(FFT * (1.0 - overlap) + 0.5);
};
