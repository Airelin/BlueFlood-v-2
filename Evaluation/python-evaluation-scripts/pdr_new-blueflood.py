import os
import progressbar
import numpy as np

import matplotlib.pyplot as plt
from utility import slugify, cached, init_cache, load_env_config

METHOD_PREFIX = 'export_'

CONFIDENCE_FILL_COLOR = '0.8'
COLOR_MAP = 'tab10'


def load_plot_defaults():
    # Configure as needed
    plt.rc('lines', linewidth=2.0)
    plt.rc('legend', framealpha=1.0, fancybox=True)
    plt.rc('errorbar', capsize=3)
    plt.rc('pdf', fonttype=42)
    plt.rc('ps', fonttype=42)
    plt.rc('font', size=11)


def export_sine_wave_example(config, export_dir):
    # We got multiple experiment runs with individual measurements
    num_runs = 10

    # Create our measurement steps
    xs = np.linspace(0, 2 * np.pi, 100, endpoint=True)

    # We also collect overall data for mean and confidence interval
    overall_data = []

    for r in range(0, num_runs):
        name = "Sine Wave Run {}".format(r)

        def proc():
            # you can load your data from a database or CSV file here
            # we will randomly generate data
            ys = np.sin(np.array(xs))
            # we add some uniform errors
            ys += np.random.uniform(-0.1, 0.1, len(xs))
            return ys

        # If caching is enabled, this line checks for available cache data
        # If no data was found, the proc callback is executed and the result cached
        # Use ys = proc() if caching not yet wanted
        ys = cached(('sine_wave', r), proc)

        # We also add the data to overall_data
        overall_data.append(ys)

        plt.clf()

        # Plot the main data
        plt.plot(xs, ys, linestyle='-', label="Sin Run {}".format(r), color='C' + str(r + 1))

        plt.legend()
        plt.xlabel("x")
        plt.ylabel("sin(x)")
        plt.axis([None, None, None, None])
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(export_dir + slugify(name) + ".pdf", format="pdf")
        plt.close()

    overall_data = np.array(overall_data)

    # We swap the axes to get all values at the first position together
    overall_data = np.swapaxes(overall_data, 0, 1)

    # We can then merge each step to get the mean
    mean = np.mean(overall_data, axis=1)

    # Calculate the lower and upper bounds of the confidence interval
    # This describes that 95% of the measurements (for each timestep) are within that range
    # Use standard error to determine the "quality" of your calculated mean
    (lq, uq) = np.percentile(overall_data, [2.5, 97.5], axis=1)

    plt.clf()
    plt.plot(xs, mean, linestyle='-', label="Mean", color='C1')
    plt.fill_between(xs, lq, uq, color=CONFIDENCE_FILL_COLOR, label='95% Confidence Interval')
    plt.legend()
    plt.xlabel("x")
    plt.ylabel("sin(x)")
    plt.axis([None, None, None, None])
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(export_dir + slugify("Sine Wave Mean") + ".pdf", format="pdf")
    plt.close()


def export_bar_example(config, export_dir):
    # we want to display two bar grahps
    # see export_sine_wave_example

    num_data_points = 100

    data_a = []
    with open('../Logs/5213_blueflood2-mode5/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_a:
        lines_a = file_a.readlines()
        str_num_a = []
        for line in lines_a:
            str_num_a.append(line)
        for num in str_num_a:
            data_a.append(float(num))
    
    data_b = []
    with open('../Logs/5214_blueflood2-mode6/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_b:
        lines_b = file_b.readlines()
        str_num_b = []
        for line in lines_b:
            str_num_b.append(line)
        for num in str_num_b:
            data_b.append(float(num))

    data_c = [] 
    with open('../Logs/5211_blueflood2-mode3/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_c:
        lines_c = file_c.readlines()
        str_num_c = []
        for line in lines_c:
            str_num_c.append(line)
        for num in str_num_c:
            data_c.append(float(num))

    data_d = []
    with open('../Logs/5212_blueflood2-mode4/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_d:
        lines_d = file_d.readlines()
        str_num_d = []
        for line in lines_d:
            str_num_d.append(line)
        for num in str_num_d:
            data_d.append(float(num))

    mean_a = np.mean(data_a)
    mean_b = np.mean(data_b)
    mean_c = np.mean(data_c)
    mean_d = np.mean(data_d)

    std_a = np.std(data_a)
    std_b = np.std(data_b)
    std_c = np.std(data_c)
    std_d = np.std(data_d)

    plt.clf()

    fig, ax = plt.subplots()

    ax.bar(["125 Kbit", "500 Kbit", "1 Mbit", "2 Mbit"], [mean_a, mean_b, mean_c, mean_d], yerr=[std_a, std_b, std_c, std_d], align='center',
           ecolor='black', capsize=5, color=['C1', 'C2', 'C3', 'C4'])
    ax.yaxis.grid(True)
    plt.ylabel("PDR in %")
    plt.axis([None, None, 0, 1])

    # Adapt the figure size as needed
    fig.set_size_inches(5.0, 8.0)
    plt.tight_layout()
    plt.savefig(export_dir + slugify(("20210921","pdr","new-blueflood","Bar", 5.0, 8.0)) + ".pdf", format="pdf")

    fig.set_size_inches(4.0, 4.0)
    plt.tight_layout()
    plt.savefig(export_dir + slugify(("20210921","pdr","new-blueflood","Bar", 4.0, 4.0)) + ".pdf", format="pdf")

    plt.close()

def export_example_3(config, export_dir):
    pass


if __name__ == '__main__':

    config = load_env_config()

    load_plot_defaults()

    assert 'EXPORT_DIR' in config and config['EXPORT_DIR']

    if 'CACHE_DIR' in config and config['CACHE_DIR']:
        init_cache(config['CACHE_DIR'])

    steps = [
        #export_sine_wave_example,  # I put the example I am working on to the beginning
        export_bar_example,
        # export_example_3,  excluded for now
    ]

    for step in progressbar.progressbar(steps, redirect_stdout=True):
        name = step.__name__.removeprefix(METHOD_PREFIX)
        print("Handling {}".format(name))
        export_dir = os.path.join(config['EXPORT_DIR'], name) + '/'
        os.makedirs(export_dir, exist_ok=True)
        step(config, export_dir)
