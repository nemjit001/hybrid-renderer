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

def create_t_barchart(df: pd.DataFrame, filename: str, title: str, ylimits: list[float], firstView: bool):
    T_LIST = [ 0.0, 0.25, 0.50, 0.75, 1.00 ]
    t_data = {
        'T Interval': [],
        'Avg Render Time (ms)': [],
    }

    for t in T_LIST:
        t_df = df[df['T Interval'] == t]
        if firstView:
            t_df = t_df[t_df['Camera Position'] == 0]

        t_data['T Interval'].append(t)
        t_data['Avg Render Time (ms)'].append(t_df["Total Render time (ms)"].mean())

    avgd_df = pd.DataFrame(t_data)
    avgd_df.plot.bar('T Interval', 'Avg Render Time (ms)')
    plt.title(title)
    plt.tight_layout()
    plt.ylim(ylimits[0], ylimits[1])
    plt.savefig(filename)

def create_t_barchart_build_time(df: pd.DataFrame, filename: str, title: str, ylimits: list[float]):
    T_LIST = [ 0.0, 0.25, 0.50, 0.75, 1.00 ]
    t_data = {
        'T Interval': [],
        'Avg AS Build Time (ms)': [],
    }

    for t in T_LIST:
        t_df = df[df['T Interval'] == t]
        t_data['T Interval'].append(t)
        t_data['Avg AS Build Time (ms)'].append(t_df["AS Build time (ms)"].mean())

    avgd_df = pd.DataFrame(t_data)
    avgd_df.plot.bar('T Interval', 'Avg AS Build Time (ms)')
    plt.title(title)
    plt.tight_layout()
    plt.ylim(ylimits[0], ylimits[1])
    plt.savefig(filename)

def hr_per_pass_bar_char(df: pd.DataFrame, filename: str, title: str):
    T_LIST = [ 0.0, 0.25, 0.50, 0.75, 1.00 ]
    t_data = {
        'T Interval': [],
        'Avg RNG Generation time (ms)': [],
        'Avg GBuffer Layout time (ms)': [],
        'Avg GBuffer Sample time (ms)': [],
        'Avg Direct Illumination time (ms)': [],
        'Avg Deferred Shading time (ms)': [],
        'Avg Reprojection time (ms)': [],
    }

    for t in T_LIST:
        t_df = df[df['T Interval'] == t]
        t_df = t_df[t_df['Camera Position'] == 0]
        t_data['T Interval'].append(t)
        t_data['Avg RNG Generation time (ms)'].append(t_df['RNG Generation time (ms)'].mean())
        t_data['Avg GBuffer Layout time (ms)'].append(t_df['GBuffer Layout time (ms)'].mean())
        t_data['Avg GBuffer Sample time (ms)'].append(t_df['GBuffer Sample time (ms)'].mean())
        t_data['Avg Direct Illumination time (ms)'].append(t_df['Direct Illumination time (ms)'].mean())
        t_data['Avg Deferred Shading time (ms)'].append(t_df['Deferred Shading time (ms)'].mean())
        t_data['Avg Reprojection time (ms)'].append(t_df['Reprojection time (ms)'].mean())

    avg_df = pd.DataFrame(t_data)
    avg_df.plot.bar('T Interval', stacked=True)
    plt.title(title)
    plt.tight_layout()
    plt.savefig(filename)

def hr_pass_time_bar_char(df: pd.DataFrame, filename: str, title: str, column: str):
    T_LIST = [ 0.0, 0.25, 0.50, 0.75, 1.00 ]
    t_data = {
        'T Interval': [],
        f'Avg {column}': [],
    }

    for t in T_LIST:
        t_df = df[df['T Interval'] == t]
        t_df = t_df[t_df['Camera Position'] == 0]
        t_data['T Interval'].append(t)
        t_data[f'Avg {column}'].append(t_df[column].mean())

    avg_df = pd.DataFrame(t_data)
    avg_df.plot.bar('T Interval', stacked=True)
    plt.title(title)
    plt.tight_layout()
    plt.savefig(filename)

def create_bar_char_performance(df_hr: pd.DataFrame, df_pt: pd.DataFrame, filename: str, title: str):
    data = {
        'Renderer': [
            'Hybrid Renderer',
            'Path Tracer',
        ],
        'Avg Render Time (ms)': [
            df_hr['Total Render time (ms)'].mean(),
            df_pt['Total Render time (ms)'].mean(),
        ]
    }

    df = pd.DataFrame(data)
    df.plot.bar()
    plt.title(title)
    plt.tight_layout()
    plt.savefig(filename)

pt_df = create_dataset(PT_FILES, PT_RP_COLS)
hr_df = create_dataset(HR_FILES, HR_RP_COLS)

create_t_barchart(hr_df, 'figs/hr_avg_rendertime.png', 'Average Render time (ms) using Hybrid Rendering', [ 5.8, 6.3 ], False)
create_t_barchart(pt_df, 'figs/pt_avg_rendertime.png', 'Average Render time (ms) using Pathtracing', [ 2.0, 2.4 ], False)

create_t_barchart(hr_df, 'figs/hr_avg_rendertime_view0.png', 'Average Render time (ms) for View 0 using Hybrid Rendering', [ 2.25, 2.75 ], True)
create_t_barchart(pt_df, 'figs/pt_avg_rendertime_view0.png', 'Average Render time (ms) for View 0 using Pathtracing', [ 1.50, 2.00 ], True)

create_t_barchart_build_time(hr_df, 'figs/hr_avg_as_build_time.png', 'Average AS Build time (ms) using Hybrid Rendering', [ 1.0, 3.0 ])
create_t_barchart_build_time(pt_df, 'figs/pt_avg_as_build_time.png', 'Average AS Build time (ms) using Pathtracing', [ 1.0, 3.0 ])

create_bar_char_performance(hr_df, pt_df, 'figs/performance_comp.png', 'Average render time for each renderer.')

hr_per_pass_bar_char(hr_df, 'figs/hr_avg_per_pass.png', 'Per pass render time for Hybrid Rendering')
hr_pass_time_bar_char(hr_df, 'figs/hr_gbuff_sample_time.png', 'GBuffer Sample pass Render time', 'GBuffer Sample time (ms)')
hr_pass_time_bar_char(hr_df, 'figs/hr_gbuff_layout_time.png', 'GBuffer Layout pass Render time', 'GBuffer Layout time (ms)')
