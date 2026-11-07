import json, os, pathlib, sys, wave
root=pathlib.Path(sys.argv[1]);out=root/'Performance-comparison.md'
labels=['v037-closed','v3-closed','v037-open','v3-open']
reports=[json.loads((root/(name+'-callback.json')).read_text(encoding='utf-8')) for name in labels]
for label in ['v037','v3']:
    for q in range(4):
        with wave.open(str(root/f'{label}-closed-quadrant-{q}.wav')) as a, wave.open(str(root/f'{label}-open-quadrant-{q}.wav')) as b:
            if a.getparams()!=b.getparams() or a.readframes(a.getnframes())!=b.readframes(b.getnframes()):raise SystemExit(f'Editor changed audio: {label} quadrant {q}')
text=['# Paced native processBlock measurements','', 'Optimized Release builds, stereo synthetic bass + DNA. The same harness calls the actual JUCE processor at buffer deadlines on a dedicated test thread. This is not an audio-device/ASIO or FL Studio capture. UI-open cases use the actual visible JUCE/WebView2 editor after 5 seconds warmup. Callback durations exclude input generation, waiting, UI work and result serialization. Observed maxima are not a worst-case guarantee; shared CI scheduling can add outliers.','']
for d in reports:
    text += [f"## {d['label']}",'',f"Hardware: {d['cpu']}; {d['logicalCpus']} logical CPUs; {d['ramMiB']} MiB system RAM; {d['os']}.",f"Editor open: {d['editorOpen']}. Fixed reported latency: {d['latencySamples']} samples. Process-tree snapshot: {d['processTreeResidentBytes']/1048576:.1f} MiB. Process-tree CPU during measurement/render sequence: {d['processTreeCpuSeconds']:.3f} s. Windows tree includes descendant WebView2 processes; working-set summation may count shared pages more than once. Per-case residentBytes is the parent process only.",'','| Rate | Buffer | Callbacks | Mean µs | p95 µs | p99 µs | Max µs | Deadline µs |','|---|---|---|---|---|---|---|---|']
    for c in d['cases']:text += [f"| {c['sampleRate']} | {c['buffer']} | {c['callbacks']} | {c['meanUs']:.2f} | {c['p95Us']:.2f} | {c['p99Us']:.2f} | {c['maxUs']:.2f} | {c['deadlineUs']:.2f} |"]
    text += ['']
text += ['Editor-open/closed audition renders are sample-identical for each version. Compare open versus closed reports for UI overhead; compare v037 versus v3 under the same UI state. CPU totals include post-timing audition generation, so use callback tables for real-time processing cost. No user reference audio is included.']
report='\n'.join(text)+'\n'
out.write_text(report,encoding='utf-8')
if os.environ.get('GITHUB_STEP_SUMMARY'):
    with pathlib.Path(os.environ['GITHUB_STEP_SUMMARY']).open('a',encoding='utf-8') as summary:summary.write(report)
print(out)
