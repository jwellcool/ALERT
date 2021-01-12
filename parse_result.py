import os
import shutil
import xlsxwriter

def mean(nums):
    sum = 0.0
    for num in nums:
        sum += num
    return sum / len(nums)

def get_y_axis(k):
    if k == 'used_memory[MB]':
        return  '索引大小（MB）'
    if k == 'build_time[s]':
        return '构建时间（s）'
    if k == 'ns/lookup':
        return '延迟（ns/lookup）'
    if k == 'ns/range':
        return '延迟（ns/range）'
    if k == 'ns/insert':
        return '延迟（ns/insert）'
    if k == 'ns/op':
        return '延迟（ns/op）'
    return ''


def parse_result(file_name):
    d = {}
    with open(file_name, 'r') as f:
        for line in f.readlines():
            kvs = line.strip().split()
            index = kvs[0].split(":")[1]
            dataset = kvs[1].split(":")[1]
            for kv in kvs[2:]:
                k, v = kv.split(":")
                if k not in d.keys():
                    d[k] = {}
                if dataset not in d[k].keys():
                    d[k][dataset] = {}
                if index not in d[k][dataset] .keys():
                    d[k][dataset][index] = []
                d[k][dataset][index].append(float(v))

    output_dir = file_name.split("/")[-1].split('.')[0]
    if os.path.exists(output_dir):
        shutil.rmtree(output_dir)
    os.mkdir(output_dir)
    for k, v in d.items():
        output_file = k.replace('/', "-")
        print(output_file, v)
        workbook = xlsxwriter.Workbook(output_dir + '/' + output_file + '.xlsx', {'nan_inf_to_errors': True})
        worksheet = workbook.add_worksheet()
        row = 0
        for dataset, content in v.items():
            worksheet.write(row, 1, dataset)
            row += 1
            start_row = row+1
            for index, results in content.items():
                worksheet.write(row, 0, index)
                worksheet.write(row, 1, round(mean(results), 2))
                row += 1

            chart = workbook.add_chart({'type': 'column'})

            # 配置第一个系列数据
            chart.add_series({
                # 'name': '=Sheet1!$B$1',
                'categories': '=Sheet1!$A$' + str(start_row) + ':$A$' + str(row),
                'values': '=Sheet1!$B$' + str(start_row) + ':$B$' + str(row),
                # 'line': {'color': 'blue'},
            })

            chart.set_y_axis({'name': get_y_axis(k)})
            chart.set_legend({'none': True})
            chart.show_hidden_data()
            chart.set_style(2)
            worksheet.insert_chart('F' + str(start_row*3), chart)

            row += 1

        workbook.close()

if __name__ == '__main__':
    for file_name in os.listdir('results'):
        parse_result('results/' + file_name)