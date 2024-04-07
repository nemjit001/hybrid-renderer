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
        df['Total Render time (ms)'] = render_time

        df_results = pd.concat([df_results, df])

    return df_results

def create_t_barchart(df: pd.DataFrame, filename: str, ylimits):
    T_LIST = [ 0.0, 0.25, 0.50, 0.75, 1.00 ]
    t_data = {
        'T Interval': [],
        'Avg Render Time (ms)': [],
    }

    for t in T_LIST:
        t_df = df[df['T Interval'] == t]
        t_data['T Interval'].append(t)
        t_data['Avg Render Time (ms)'].append(t_df["Total Render time (ms)"].mean())

    avgd_df = pd.DataFrame(t_data)
    avgd_df.plot.bar('T Interval', 'Avg Render Time (ms)')
    plt.ylim(ylimits[0], ylimits[1])
    plt.savefig(filename)

pt_df = create_dataset(PT_FILES, PT_RP_COLS)
hr_df = create_dataset(HR_FILES, HR_RP_COLS)

create_t_barchart(hr_df, 'hr_avg_rendertime.png', [ 6.0, 6.2 ])
create_t_barchart(pt_df, 'pt_avg_rendertime.png', [ 2.1, 2.3 ])

# TODO: calculate mean render time & as build time per cam position per interval
# TODO: create figures for total render time & as build time, per t interval, per cam position
# TODO: create comparison between render method times
