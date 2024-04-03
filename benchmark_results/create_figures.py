import glob
import pandas as pd
import matplotlib.pyplot as plt

HR_FILES = glob.glob('dataset/HybridRendering_*.csv')
PT_FILES = glob.glob('dataset/PathTracing_*.csv')

HR_RP_COLS = [ 'RNG Generation time (ms)', 'GBuffer Layout time (ms)', 'GBuffer Sample time (ms)', 'Direct Illumination time (ms)', 'Deferred Shading time (ms)', 'Reprojection time (ms)']
PT_RP_COLS = [ 'RNG Generation time (ms)', 'Path Tracing time (ms)', 'Reprojection time (ms)' ]

def create_dataset(files, renderPassColumns: list[str]):
    df_results = pd.DataFrame(columns=[])
    for file in files:
        print(file)
        df = pd.read_csv(file)

        render_time = df[renderPassColumns].sum(axis=1)
        df["Total Render time (ms)"] = render_time

        df_results = pd.concat([df_results, df])

    return df_results

pt_df = create_dataset(PT_FILES, PT_RP_COLS)
hr_df = create_dataset(HR_FILES, HR_RP_COLS)

# TODO: calculate mean render time & as build time per cam position per interval
# TODO: create figures for total render time & as build time, per t interval, per cam position
# TODO: create comparison between render method times
