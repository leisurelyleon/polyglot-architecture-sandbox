-- 1. Create a table representing a strict graph structure (Nodes and Edges)
CREATE TABLE bill_of_materials (
    part_id VARCHAR(50) PRIMARY KEY,
    part_name VARCHAR(100) NOT NULL,
    parent_part_id VARCHAR(50) REFERENCES bill_of_materials(part_id),
    quantity_required INT NOT NULL DEFAULT 1,
    unit_cost NUMERIC(10, 2) NOT NULL
);

INSERT INTO bill_of_materials VALUES
    ('ENG-001', 'V8 Engine Assembly', NULL, 1, 0.00),
    ('BLK-100', 'Engine Block', 'ENG-001', 1, 1500.00),
    ('PST-200', 'Piston Set', 'ENG-001', 8, 45.00),
    ('RNG-201', 'Piston Rings', 'PST-200', 3, 5.00),  -- 3 rings per piston
    ('PIN-202', 'Wrist Pin', 'PST-200', 1, 12.00);     -- 1 pin per piston

-- 2. The Recursive Common Table Expression (CTE)
WITH RECURSIVE assembly_tree AS (
    -- A: The Anchor Member (Start at the top of the tree: The Engine)
    SELECT 
        part_id, 
        part_name, 
        parent_part_id, 
        quantity_required, 
        unit_cost,
        quantity_required AS total_quantity_needed,
        (quantity_required * unit_cost) AS total_node_cost,
        1 AS assembly_depth,
        CAST(part_id AS VARCHAR(500)) AS assembly_path -- Track the exact lineage
    FROM bill_of_materials
    WHERE parent_part_id IS NULL

    UNION ALL

    -- B: The Recursive Member (Joins against the CTE itself to walk down the tree)
    SELECT 
        child.part_id, 
        child.part_name, 
        child.parent_part_id, 
        child.quantity_required, 
        child.unit_cost,
        -- Multiply child requirement by the parent's total requirement
        (child.quantity_required * parent.total_quantity_needed) AS total_quantity_needed,
        ((child.quantity_required * parent.total_quantity_needed) * child.unit_cost) AS total_node_cost,
        parent.assembly_depth + 1 AS assembly_depth,
        CAST(parent.assembly_path || ' -> ' || child.part_id AS VARCHAR(500)) AS assembly_path
    FROM bill_of_materials child
    INNER JOIN assembly_tree parent ON child.parent_part_id = parent.part_id
)

-- 3. Execute the final aggregation, rolling up the massive hierarchical graph into flat analytics
SELECT 
    part_id,
    part_name,
    assembly_depth,
    assembly_path,
    total_quantity_needed,
    total_node_cost,
    -- Window Function to get the grand total cost of the entire engine
    SUM(total_node_cost) OVER () AS total_assembly_cost
FROM assembly_tree
ORDER BY assembly_path;
