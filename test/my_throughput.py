#encoding: utf-8
import matplotlib.pyplot as plt
import sys
import numpy as np

path='/vagrant/tju_tcp/test/trace64/'

font_size = 15

def plot_throughput(time_list, throuput_list, thrp_intv):
    plt.plot(time_list, throuput_list, color='black')
    plt.xlabel('Time (s)', fontdict={'size':font_size})
    plt.ylabel('throuput (Mbps)', fontdict={'size':font_size})
    plt.ylim(ymin=0, ymax=max(throuput_list)*1.05)
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(path+'Throuput.png', dpi=600)
    print("绘制成功, 图像位于/vagrant/tju_tcp/test/Throuput.png [注: 每%.3fs计算一次瞬时吞吐率]"%thrp_intv)
    plt.cla()

# 只读取 DELV 事件
def read_trace(file):
    DELV_dic = {'utctime':[], 'seq':[], 'size':[], 'throughput':[]}

    start_time = 0
    with open(file, 'r', encoding='utf-8') as f:
        for num, line in enumerate(f):
            if(line=='\n'): continue # 跳过空行
            if('DELV' not in line): continue # 跳过非[DELV]事件行
            line = line.strip('\n')
            line = line.replace('[', '')
            line = line.replace(']', '')
            line_list = line.split(' ')
            info_list = line_list[2:]
            info_list = [item.split(':')[1] for item in info_list]
            
            DELV_dic['utctime'].append(int(line_list[0]))
            DELV_dic['seq'].append(int(info_list[0]))
            DELV_dic['size'].append(int(info_list[1])) 

            if start_time==0:
                start_time = int(line_list[0])

    DELV_dic['time'] = [item - start_time for item in DELV_dic['utctime']]
    DELV_dic['time'] = np.divide(DELV_dic['time'], 1000000)

    return DELV_dic

FILE_TO_READ = path+'server.event.trace'
if len(sys.argv)>=2:
	FILE_TO_READ = sys.argv[1]
print("正在使用 %s Trace文件绘图"%FILE_TO_READ)
DELV_dic = read_trace(FILE_TO_READ)


# 每间隔1s绘制一次吞吐率
if len(DELV_dic['utctime']): 
    thrp_intv = 1 # throughput interval
    time_start = DELV_dic['time'][0]
    intvs = int(DELV_dic['time'][-1])
    thrp_list = []
    for i in range(intvs):
        time_end = time_start+thrp_intv
        mark = (DELV_dic['time']>=time_start) & (DELV_dic['time']<time_end)
        payloads_size = np.array(DELV_dic['size'])[mark]
        DELV_dic['throughput'].append(np.sum(payloads_size)*8/thrp_intv*0.000001) # 单位: Mbps
        time_start += thrp_intv
    plot_throughput(range(intvs), DELV_dic['throughput'], thrp_intv)
