import os
import csv

def aggregate_data_no_pandas():
    """
    Aggregates data from various files in the 'data/entities' directory
    into a single CSV file without using the pandas library.
    """
    data_dir = os.path.dirname(os.path.abspath(__file__))
    entities_dir = os.path.join(data_dir, 'entities')
    output_file = os.path.join(data_dir, 'aggregated_data.csv')

    all_data = []
    all_fieldnames = {'entity_id', 'source_file'}

    entity_dirs = [d for d in os.listdir(entities_dir) if os.path.isdir(os.path.join(entities_dir, d))]

    for entity in entity_dirs:
        entity_path = os.path.join(entities_dir, entity)
        for root, _, files in os.walk(entity_path):
            for file in files:
                file_path = os.path.join(root, file)
                source_file = os.path.relpath(file_path, entities_dir)

                if file.endswith('.csv'):
                    try:
                        with open(file_path, 'r', newline='') as f:
                            reader = csv.DictReader(f)
                            for row in reader:
                                row['entity_id'] = entity
                                row['source_file'] = source_file
                                all_data.append(row)
                                all_fieldnames.update(row.keys())
                    except Exception as e:
                        print(f"Error reading {file_path}: {e}")
                elif file == 'metadata.txt':
                    try:
                        with open(file_path, 'r') as f:
                            metadata = {'entity_id': entity, 'source_file': source_file}
                            for line in f:
                                if ':' in line:
                                    key, value = line.strip().split(':', 1)
                                    metadata[key.strip()] = value.strip()
                            all_data.append(metadata)
                            all_fieldnames.update(metadata.keys())
                    except Exception as e:
                        print(f"Error reading {file_path}: {e}")

    if all_data:
        # Sort fieldnames for consistent column order, with entity_id and source_file first
        sorted_fieldnames = sorted([f for f in all_fieldnames if f is not None])
        if 'source_file' in sorted_fieldnames:
            sorted_fieldnames.remove('source_file')
            sorted_fieldnames.insert(0, 'source_file')
        if 'entity_id' in sorted_fieldnames:
            sorted_fieldnames.remove('entity_id')
            sorted_fieldnames.insert(0, 'entity_id')

        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=sorted_fieldnames)
            writer.writeheader()
            for row in all_data:
                if None in row:
                    del row[None]
            writer.writerows(all_data)
        print(f"Aggregated data saved to {output_file}")
    else:
        print("No data to aggregate.")

if __name__ == '__main__':
    aggregate_data_no_pandas()