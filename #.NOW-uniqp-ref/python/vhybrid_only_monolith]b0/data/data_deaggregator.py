import pandas as pd
import os
import numpy as np

def deaggregate_data():
    """
    Reads the aggregated_data.csv file and splits the data back into
    the original entity directory structure.
    """
    aggregated_file = 'aggregated_data.csv'
    output_base_dir = 'entities'

    if not os.path.exists(aggregated_file):
        print(f"'{aggregated_file}' not found. Please generate it first.")
        return

    try:
        df = pd.read_csv(aggregated_file)
    except Exception as e:
        print(f"Error reading {aggregated_file}: {e}")
        return

    # Replace numpy NaN with None for easier handling
    df = df.replace({np.nan: None})

    grouped = df.groupby('source_file')

    for source_file, group in grouped:
        output_path = os.path.join(output_base_dir, source_file)
        
        # Create directories if they don't exist
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        # Drop columns that are all None for this group
        group_cleaned = group.dropna(axis=1, how='all')
        
        # Remove the helper columns
        if 'entity_id' in group_cleaned.columns:
            group_cleaned = group_cleaned.drop(columns=['entity_id'])
        if 'source_file' in group_cleaned.columns:
            group_cleaned = group_cleaned.drop(columns=['source_file'])
        
        if source_file.endswith('.csv'):
            try:
                group_cleaned.to_csv(output_path, index=False)
                print(f"Wrote {output_path}")
            except Exception as e:
                print(f"Error writing {output_path}: {e}")

        elif os.path.basename(source_file) == 'metadata.txt':
            try:
                with open(output_path, 'w') as f:
                    # For metadata, we take the first row and write key-value pairs
                    if not group_cleaned.empty:
                        first_row = group_cleaned.iloc[0]
                        for col, value in first_row.items():
                            if value is not None:
                                f.write(f"{col}: {value}\n")
                        print(f"Wrote {output_path}")
            except Exception as e:
                print(f"Error writing {output_path}: {e}")
        else:
            # For any other .txt files, just write the first column's content
            try:
                with open(output_path, 'w') as f:
                    if not group_cleaned.empty:
                        first_col_name = group_cleaned.columns[0]
                        for value in group_cleaned[first_col_name]:
                            if value is not None:
                                f.write(f"{value}\n")
                        print(f"Wrote {output_path}")
            except Exception as e:
                print(f"Error writing {output_path}: {e}")

if __name__ == '__main__':
    deaggregate_data()
