# Native opaque-material denoise guidance

Main Edit adds material and sampling guidance to the existing Disney V2 denoiser.
The render request still uses `render.denoise_enabled`; no extra scene preset or
texture scaling is required. Disney V1 and surfaces without valid material guides
retain the existing filter. This is a development candidate, not installed or
released behavior.

The feature buffer retains the primary-hit normal for surface identity and also
records evaluated linear base color, the final microdetail shading normal, the
maximum validated raw RGB variance of the mean, per-pixel sample count and a material-guide validity
mask. Frame-wide gathering transports these fields across render tiles. Unit-local
preview resolution uses the same sampling-statistics conversion.

The current native camera path traces the same primary ray for every lighting
subpass, so the guides match its beauty footprint. Camera jitter, motion blur or
coverage sampling must accumulate guides with beauty before using this contract.
The count is the actual per-pixel count, not the requested frame ceiling.

For valid guided opaque surfaces, zero or sufficiently low measured uncertainty
preserves the original pixel. Fewer than four samples or invalid variance also
preserve it. Otherwise reconstruction blends the guided neighborhood result with the
original value, capped at 90%. Neighbor weights additionally consider evaluated
base color, roughness and the adjusted shading normal. Existing object, depth,
strong beauty-edge and transparent/mirror/glossy safeguards remain in place.
At blend strengths of at least 0.5, guided pixels search a 9×9 neighborhood
with spatial sigma 2.5; lower uncertainty and legacy surfaces retain the 5×5
search with sigma 1.25. The wider search retains the same object, depth,
normal and beauty-edge gates. Tighter color matching avoids mixing slightly
different grain colors merely to obtain more samples. Latest-sample activity is retained as a legacy fallback, not used as variance
for the guided path.

Raw variance is calculated from Welford M2 as `M2 / (n * (n - 1))`. It describes
the unfiltered samples. Because resolved beauty uses firefly clamping, this is
an uncertainty proxy rather than an exact variance of that clamped estimator.
The initial policy uses a luminance-dependent target of `0.002 + 0.01 * abs(L)`,
preserves pixels below half that standard-error target, and uses continuous
material weights with color sigma 0.006, roughness sigma 0.08 and shading-normal
dot exponent 2048. These are tested development policy values, not universal
material or photometric constants.

Mixed volume rendering disables the new material-guide path. Its existing
denoise behavior remains unchanged until volume and surface signals can be
handled separately. Sharp reflection and transmission reconstruction, material
class IDs, animation reprojection and a mature external denoise backend remain
outside this slice.

Qualification uses clean/noisy texture controls, nonfinite and insufficient
sampling cases, unequal per-pixel counts, 8/16/32 tile partitions and isolated
wood/floor render comparisons. Fine-detail preservation improves substantially
over the prior filter. The initial material views show little additional noise
benefit over denoise off; broad final-quality acceptance is still pending.

A bounded fixed-sample follow-up tightens base-color sigma from 0.015 to 0.006
and widens the search only for guided blend strengths >= 0.5. On the native
eight-sample wood/floor coupons against retained 128-sample raw references,
linear RGB reference error decreases approximately 35% for wood and 0.7% for
floor versus unfiltered beauty. Adjacent-detail slopes remain approximately
99.8% and 99.9%. Wider search without tighter color matching was rejected.
Earlier cleaner views remain effectively unchanged (wood reference error
increases 0.04% against the prior candidate). These finite-reference results do
not establish universal noise reduction, physical calibration or release
acceptance. Glass, volumes, animation and installed worker qualification remain
separate.

## Guide allocation and lifetime

The legacy feature arrays cost 38 bytes per pixel. They still serve temporal
measurement and adaptive sampling when filtering is disabled. Material guides
are allocated only for eligible Disney V2 denoise, excluding active mixed-volume
transport and temporal-budget heatmap output. Generic feature allocation defaults
to legacy arrays. Changing modes and returning a unit to the reusable cache
release all material guides, including cancellation paths.

Tile guides own linear RGB albedo, float32 shading normals and a validity byte
(25 bytes per pixel). Sample counts and raw Welford moments are borrowed from
the tile's existing accumulation buffers. Borrowed data is neither cleared nor
freed by the feature buffer and remains valid through filtering. The maximum
validated RGB variance of mean is calculated from those existing moments with
the same arithmetic and invalid-value policy as the previous scalar array.

Full-frame gather allocates no albedo, shading-normal, validity, variance or
sample-count arrays. A compact sorted row-span index borrows the completed tiles.
Regular rows resolve the tile directly; irregular partitions use a bounded binary
search. The index scales with image height times the number of tile columns,
not with the full image pixel count. Filtering still uses one immutable full-frame
radiance input and crosses tile boundaries through guide views. Float precision,
filter weights, neighborhoods and thresholds are unchanged.

The frame and source tile objects must remain at stable addresses, with all guide
and accumulation arrays alive and immutable until Apply returns. Scheduler workers
are joined before gather. Views are detached before tile guides are released after
Apply, including failure paths; returning a unit to the reusable cache releases
its guides. Frame destruction frees only its index and its own legacy/radiance
arrays, never borrowed tile storage. Ordinary callers require complete coverage.
The occupancy-aware scheduler may explicitly allow sparse guide coverage for
proven-empty tiles; missing locations resolve to no material guide and retain
cleared hit masks. Overlapping/out-of-bounds spans are rejected.

Assuming full-image coverage and 16-pixel tiles, added guide storage including
index capacity is about 53.4 MiB at 1080p and 213.8 MiB at 4K. With 64-pixel tiles
it is about 50.4/201.8 MiB. These are allocation calculations confirmed by a local
allocator probe, not whole-renderer or Linux RSS measurements. The legacy 38-byte
feature arrays and other accumulation/radiance storage remain separate costs.
Denoise-off and ineligible modes add no material-guide storage.

Qualification includes exact float equivalence against contiguous guides across
horizontal/vertical seams, reversed completion, irregular edge tiles, invalid
variance, unequal sample counts, sparse occupancy and coverage refusals. Four
retained wood/floor AOV inputs replay byte-identically. Fresh before/after native
wood/floor BMPs match byte-for-byte. Targeted sanitizer and contained headless
smoke checks pass. Shared lookup adds a small filter-only CPU cost in local debug
probes; Linux optimized total-render timing, RSS and concurrency remain separate
release qualification requirements. No lower-precision guide packing is used.
